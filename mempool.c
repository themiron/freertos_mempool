/*
 * Copyright (c) 2019 Vladislav Grishenko <themiron@mail.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:

 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.

 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "mempool.h"

/* MEMPOOL_PREMAP allows either to premap block pointers within a pool on
 * pool creating stage or to lazily map them one-by-one while allocation.
 * Disabling premap is not really useful unless you have multiple dynamic
 * pools with minor block count. */
#ifndef MEMPOOL_PREMAP
#define MEMPOOL_PREMAP 1
#endif

/* MEMPOOL_FREEPTR allows to free allocated blocks with MemPoolFreePtr by
 * by block pointer w/o specifying exact pool handle. Can be useful with
 * multiple pools and one desttuction code point.
 */
#ifndef MEMPOOL_FREEPTR
#define MEMPOOL_FREEPTR 1
#endif

/* MEMPOOL_ISR controls interrupts while pool locking. Needs to be enabled
 * if pools are about to be used from interrup context, in other case only
 * thread schduler will be disabled.
 */
#ifndef MEMPOOL_ISR
#define MEMPOOL_ISR 1
#endif

#define MEMPOOL_ALIGN(x) (((portPOINTER_SIZE_TYPE)(x) + (portBYTE_ALIGNMENT) - 1) & ~((portPOINTER_SIZE_TYPE)(portBYTE_ALIGNMENT_MASK)))
#if MEMPOOL_ISR
#define MEMPOOL_LOCK() taskENTER_CRITICAL()
#define MEMPOOL_UNLOCK() taskEXIT_CRITICAL()
#else
#define MEMPOOL_LOCK() vTaskSuspendAll()
#define MEMPOOL_UNLOCK() xTaskResumeAll()
#endif

/* Private item struct */
struct MemItem {
	struct MemItem * volatile next;	/* Next free item or pull */
/*	uint8_t data[0]; */
};

/* Private pool struct */
struct MemPool {
	struct MemItem * volatile next;	/* Next free item */
	size_t volatile count;		/* Item count */
	size_t isize;			/* Item size */
	unsigned int allocated:1;	/* Can be freed flag */
/*	struct MemItem item[0]; */
};

/* Generate build error on static struct mismatch */
struct BUG_bad_sizeof_StaticMemPool {
	char BUG[sizeof(StaticMemPool_t) >= sizeof(struct MemPool) ? 1 : -1];
};
struct BUG_bad_sizeof_StaticMemItem {
	char BUG[sizeof(StaticMemItem_t) >= sizeof(struct MemItem) ? 1 : -1];
};

MemPool_t MemPoolCreate(size_t ItemSize, size_t ItemCount)
{
    struct MemPool *pool;
    struct MemItem *item;
#if MEMPOOL_PREMAP
    struct MemItem *next;
#endif
    size_t hsize, isize;

    if (ItemSize == 0 || ItemCount == 0) {
	return NULL;
    }

    hsize = MEMPOOL_ALIGN(sizeof(struct MemPool));
    isize = MEMPOOL_ALIGN(sizeof(struct MemItem)) + MEMPOOL_ALIGN(ItemSize);
    pool = (struct MemPool *)pvPortMalloc(hsize + ItemCount*isize);
    if (!pool) {
	return NULL;
    }

    memset(pool, 0, sizeof(struct MemPool));
    pool->count = ItemCount;
    pool->allocated = -1;

    item = (struct MemItem *)((uint8_t *)pool + hsize);
    item->next = NULL;
#if MEMPOOL_PREMAP
    for (; --ItemCount; item = next) {
	next = (struct MemItem *)((uint8_t *)item + isize);
	next->next = item;
    }
#else
    pool->isize = isize;
#endif
    pool->next = item;

    return pool;
}

MemPool_t MemPoolCreateStatic(size_t ItemSize, size_t ItemCount, void *Buffer, size_t BufferSize, StaticMemPool_t *MemPoolBuffer)
{
    struct MemPool pool;
    struct MemItem *item;
#if MEMPOOL_PREMAP
    struct MemItem *next;
#endif
    size_t isize;

    configASSERT(MemPoolBuffer);

    if (ItemSize == 0 || !Buffer) {
	return NULL;
    }

    isize = MEMPOOL_ALIGN(sizeof(struct MemItem)) + MEMPOOL_ALIGN(ItemSize);
    item = (struct MemItem *)MEMPOOL_ALIGN(Buffer);
    BufferSize -= (uint8_t *)item - (uint8_t *)Buffer;
    if (ItemCount == 0) {
	ItemCount = BufferSize / isize;
    }
    if (ItemCount == 0 || BufferSize < ItemCount*isize) {
	return NULL;
    }

    memset(&pool, 0, sizeof(pool));
    pool.count = ItemCount;
    pool.allocated = 0;

    item->next = NULL;
#if MEMPOOL_PREMAP
    for (; --ItemCount; item = next) {
	next = (struct MemItem *)((uint8_t *)item + isize);
	next->next = item;
    }
#else
    pool.isize = isize;
#endif
    pool.next = item;

    return (struct MemPool *)memcpy(MemPoolBuffer, &pool, sizeof(pool));
}

void MemPoolDelete(MemPool_t pool)
{
    if (!pool || !pool->allocated) {
	return;
    }

    MEMPOOL_LOCK();
    {
	pool->allocated = 0;
	pool->count = 0;
	pool->next = NULL;
	vPortFree(pool);
    }
    MEMPOOL_UNLOCK();
}

void *MemPoolAlloc(MemPool_t pool)
{
    struct MemItem *item;

    configASSERT(pool);

    MEMPOOL_LOCK();
    if (pool->count) {
	item = (struct MemItem *)pool->next;
	configASSERT(item);
#if !MEMPOOL_PREMAP
	if (pool->count && !item->next) {
	    item->next = (struct MemItem *)((uint8_t *)item + pool->isize);
	    item->next->next = NULL;
	}
#endif
	pool->next = item->next;
	pool->count--;
    } else {
	item = NULL;
    }
    MEMPOOL_UNLOCK();

    if (!item) {
	return NULL;
    }

#if MEMPOOL_FREEPTR
    item->next = (struct MemItem *)pool;
#endif
    return (uint8_t *)item + MEMPOOL_ALIGN(sizeof(struct MemItem));
}

void MemPoolFree(MemPool_t pool, void *ptr)
{
    struct MemItem *item;

    configASSERT(pool);

    if (!ptr) {
	return;
    }

    item = (struct MemItem *)((uint8_t *)ptr - MEMPOOL_ALIGN(sizeof(struct MemItem)));
    configASSERT(item);
#if MEMPOOL_FREEPTR
    configASSERT((struct MemPool *)item->next == pool);
#endif

    MEMPOOL_LOCK();
    {
	item->next = pool->next;
	pool->next = item;
	pool->count++;
    }
    MEMPOOL_UNLOCK();
}

void MemPoolFreePtr(void *ptr)
{
#if MEMPOOL_FREEPTR
    struct MemPool *pool;
    struct MemItem *item;

    if (!ptr) {
	return;
    }

    item = (struct MemItem *)((uint8_t *)ptr - MEMPOOL_ALIGN(sizeof(struct MemItem)));
    configASSERT(item);
    pool = (struct MemPool *)item->next;
    configASSERT(pool);

    MEMPOOL_LOCK();
    {
	item->next = pool->next;
	pool->next = item;
	pool->count++;
    }
    MEMPOOL_UNLOCK();
#else
    configASSERT(!ptr);
#endif
}

size_t MemPoolAvailable(MemPool_t pool)
{
    int count;

    if (!pool) {
	return 0;
    }

    count = pool->count;

    return count;
}
