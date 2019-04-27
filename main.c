/*
 * Copyright (c) 2019 Vladislav Grishenko <themiron@mail.ru>
 *
 * Threading stress partially based on FreeRTOS Add-ons project.
 * Source Code:
 * https://github.com/michaelbecker/freertos-addons
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
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "mempool.h"

typedef struct {
    int Index;
    int Delay;
    int Pattern;
} ThreadParameters_t;

#define MEMPOOL_ITEM_COUNT 32
#define MEMPOOL_ITEM_SIZE  256
#define MEMPOOL_POOL_COUNT 8
#define MEMPOOL_THREAD_COUNT 16

#define TEST(n, e, a)				\
    printf("Test %d.%d:\n", n >> 8, n & 0xff);	\
    (e);					\
    printf("\t%s\n", a ? "PASSED" : "FAILED");	\
    configASSERT(a);

static MemPool_t pool;
static MemPool_t pools[MEMPOOL_POOL_COUNT];
static StaticMemPool_t StaticPool;
static uint8_t Buffer[MEMPOOL_ITEM_COUNT * (MEMPOOL_ITEM_SIZE + sizeof(StaticMemItem_t))];
static int primes[MEMPOOL_THREAD_COUNT] = { 1, 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47 };

static MemPool_t TestMemPoolCreate(size_t ItemSize, size_t ItemCount)
{
    MemPool_t pool;

    printf("\t%s(ItemSize=%lu, ItemCount=%lu)", __func__ + 4,
	ItemSize, ItemCount);
    pool = MemPoolCreate(ItemSize, ItemCount);
    printf("=%p\n", (void *)pool);
    return pool;
}

static MemPool_t TestMemPoolCreateStatic(size_t ItemSize, size_t ItemCount, void *Buffer, size_t BufferSize, StaticMemPool_t *MemPoolBuffer)
{
    printf("\t%s(ItemSize=%lu, ItemCount=%lu, Buffer=%p, BufferSize=%lu, MemPoolBuffer=%p)", __func__ + 4,
	ItemSize, ItemCount, Buffer, BufferSize, (void *)MemPoolBuffer);
    pool = MemPoolCreateStatic(ItemSize, ItemCount, Buffer, BufferSize, MemPoolBuffer);
    printf("=%p\n", (void *)pool);
    return pool;
}

static void TestMemPoolDelete(MemPool_t pool)
{
    printf("\t%s(pool=%p)\n", __func__ + 4,
	(void *)pool);
    MemPoolDelete(pool);
}

static void *TestMemPoolAlloc(MemPool_t pool)
{
    void *ptr;

    printf("\t%s(pool=%p)", __func__ + 4,
	(void *)pool);
    ptr = MemPoolAlloc(pool);
    printf("=%p\n", ptr);
    return ptr;
}

static void TestMemPoolFree(MemPool_t pool, void *ptr)
{
    printf("\t%s(pool=%p, ptr=%p)\n", __func__ + 4,
	(void *)pool, ptr);
    MemPoolFree(pool, ptr);
}

static void TestMemPoolFreePtr(void *ptr)
{
    printf("\t%s(ptr=%p)\n", __func__ + 4,
	ptr);
    MemPoolFreePtr(ptr);
}

static size_t TestMemPoolAvailable(MemPool_t pool)
{
    size_t count;

    printf("\t%s(pool=%p)", __func__ + 4,
	(void *)pool);
    count = MemPoolAvailable(pool);
    printf("=%lu\n", count);
    return count;
}

static void TestPool(MemPool_t pool, int ItemSize, int ItemCount, int Pattern)
{
    unsigned char **addr;
    int pattern;
    int i, j;

    addr = pvPortMalloc(sizeof(*addr) * ItemCount);
    configASSERT(addr);

    pattern = Pattern;
    for (i = 0; i < ItemCount; i++) {
	addr[i] = MemPoolAlloc(pool);
	if (addr[i]) {
	    memset(addr[i], pattern, ItemSize);
	    pattern++;
	} else {
	    printf("pool=%p ItemSize=%d Item=%d available=%ld allocation failure\n",
		(void *)pool, ItemSize, i, MemPoolAvailable(pool));
	}
	vTaskDelay(1);
    }

    pattern = Pattern;
    for (i = 0; i < ItemCount; i++) {
	if (!addr[i])
	    continue;
	for (j = 0; j < ItemSize; j++) {
	    configASSERT(addr[i][j] == pattern);
	}
	pattern++;
	memset(addr[i], 0xEE, ItemSize);
	MemPoolFree(pool, addr[i]);
	addr[i] = NULL;
	vTaskDelay(1);
    }

    vPortFree(addr);
}

static void TestThread(void *parameters)
{
    ThreadParameters_t *tp;
    int i, finished = 0;

    tp = (ThreadParameters_t *)parameters;

    printf("Thread %d started, pattern 0x%02x\n", tp->Index, tp->Pattern);

    vTaskDelay(tp->Delay);

    while (1) {
	for (i = 0; i < MEMPOOL_POOL_COUNT; i++) {
	    TestPool(pools[i], i + 1, 2, tp->Pattern); 
	}
	if (++finished % 100 == 0)
	    printf("Thread %d finished %d times\n", tp->Index, finished);
	vTaskDelay(1);
    }
}

int main(void)
{
    BaseType_t rc;
    ThreadParameters_t params[MEMPOOL_THREAD_COUNT];
    void *ptr1, *ptr2, *ptr3;
    int i, count, count1, count2, count3;

    /* pool == NULL */
/* Asserted
    TEST(0x101,
	pool = TestMemPoolCreateStatic(1, 1, Buffer, sizeof(Buffer), NULL),
	!pool); */
    TEST(0x102,
	TestMemPoolDelete(NULL),
	1);
/* Asserted
    TEST(0x103,
	ptr = TestMemPoolAlloc(NULL),
	!ptr); */
/* Asserted
    TEST(0x104,
	TestMemPoolFree(NULL, (void *)-1),
	1); */
    TEST(0x105,
	count = TestMemPoolAvailable(NULL),
	count == 0);

    /* Buffer */
    TEST(0x201,
	pool = TestMemPoolCreateStatic(1, 1, NULL, sizeof(Buffer), &StaticPool),
	!pool);
    TEST(0x202, (
	pool = TestMemPoolCreateStatic(1, 1, Buffer, sizeof(Buffer), &StaticPool),
	count = TestMemPoolAvailable(pool)),
	pool && count == 1);

    /* ItemSize */
    TEST(0x201,
	pool = TestMemPoolCreate(0, 1),
	!pool);
    TEST(0x202,
	pool = TestMemPoolCreateStatic(0, 1, Buffer, sizeof(Buffer), &StaticPool),
	!pool);
    TEST(0x303,
	pool = TestMemPoolCreateStatic(MEMPOOL_ITEM_SIZE + 1, MEMPOOL_ITEM_COUNT, Buffer, sizeof(Buffer), &StaticPool),
	!pool);

    /* ItemCount */
    TEST(0x301,
	pool = TestMemPoolCreate(1, 0),
	!pool);
    TEST(0x302, (
	pool = TestMemPoolCreateStatic(1, 0, Buffer, sizeof(Buffer), &StaticPool),
	count = TestMemPoolAvailable(pool)),
	pool && count > 0);
    TEST(0x303,
	pool = TestMemPoolCreateStatic(MEMPOOL_ITEM_SIZE, MEMPOOL_ITEM_COUNT + 1, Buffer, sizeof(Buffer), &StaticPool),
	!pool);

    /* Create */
    TEST(0x401, (
	pool = TestMemPoolCreate(MEMPOOL_ITEM_SIZE, MEMPOOL_ITEM_COUNT),
	count = TestMemPoolAvailable(pool),
	TestMemPoolDelete(pool)),
	pool && count == MEMPOOL_ITEM_COUNT);
    TEST(0x402, (
	pool = TestMemPoolCreateStatic(MEMPOOL_ITEM_SIZE, MEMPOOL_ITEM_COUNT, Buffer, sizeof(Buffer), &StaticPool),
	count = TestMemPoolAvailable(pool),
	TestMemPoolDelete(pool)),
	pool && count == MEMPOOL_ITEM_COUNT);

    /* Alloc */
    TEST(0x501, (
        pool = TestMemPoolCreateStatic(MEMPOOL_ITEM_SIZE, 2, Buffer, sizeof(Buffer), &StaticPool),
	count = TestMemPoolAvailable(pool)),
	pool && count == 2);
    TEST(0x502, (
	ptr1 = TestMemPoolAlloc(pool),
	count1 = TestMemPoolAvailable(pool),
	ptr2 = TestMemPoolAlloc(pool),
	count2 = TestMemPoolAvailable(pool),
	ptr3 = TestMemPoolAlloc(pool),
	count3 = TestMemPoolAvailable(pool)),
	ptr1 && ptr2 && ptr1 != ptr2 && !ptr3 && count1 == 1 && count2 == 0 && count3 == 0);
    TEST(0x503, (
	TestMemPoolFree(pool, ptr1),
	count1 = TestMemPoolAvailable(pool),
	TestMemPoolFree(pool, ptr2),
	count2 = TestMemPoolAvailable(pool),
	TestMemPoolFree(pool, ptr3),
	count3 = TestMemPoolAvailable(pool)),
	count1 == 1 && count2 == 2 && count3 == 2);
    TEST(0x504, (
	ptr1 = TestMemPoolAlloc(pool),
	count1 = TestMemPoolAvailable(pool),
	ptr2 = TestMemPoolAlloc(pool),
	count2 = TestMemPoolAvailable(pool),
	ptr3 = TestMemPoolAlloc(pool),
	count3 = TestMemPoolAvailable(pool)),
	ptr1 && ptr2 && ptr1 != ptr2 && !ptr3 && count1 == 1 && count2 == 0 && count3 == 0);
    TEST(0x505, (
	TestMemPoolFreePtr(ptr1),
	count1 = TestMemPoolAvailable(pool),
	TestMemPoolFreePtr(ptr2),
	count2 = TestMemPoolAvailable(pool),
	TestMemPoolFreePtr(ptr3),
	count3 = TestMemPoolAvailable(pool)),
	count1 == 1 && count2 == 2 && count3 == 2);

    TEST(0x601, i = 0, 1);
    for (i = 0; i < MEMPOOL_POOL_COUNT; i++) {
	pools[i] = TestMemPoolCreate(i + 1, MEMPOOL_THREAD_COUNT);
	configASSERT(pools[i]);
    }
    for (i = 0; i < MEMPOOL_THREAD_COUNT; i++) {
	params[i].Index = i + 1;
	params[i].Delay = primes[i];
	params[i].Pattern = primes[i];
	rc = xTaskCreate(TestThread, "test", 1000, (void *)&params[i], 3, NULL);
	configASSERT(rc == pdPASS);
    }

    vTaskStartScheduler();

    return 0;
}

void vAssertCalled(const char * const file, unsigned long line, const char * const func)
{
    printf("ASSERT: %s:%lu %s()\n", file, line, func);
    while(1);
}

unsigned long ulGetRunTimeCounterValue(void)
{
    return 0;
}

void vConfigureTimerForRunTimeStats(void)
{
    return;
}

void vApplicationMallocFailedHook(void)
{
    printf("Alloc Failed");
    while(1);
}
