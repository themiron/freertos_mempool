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

#ifndef MEMPOOL_H_
#define MEMPOOL_H_

/* MemPool_t
 * Pool handle.
 */
struct MemPool;
typedef struct MemPool * MemPool_t;

/* StaticMemPool_t
 * Static pool buffer.
 */
typedef struct {
	void *dummy1;
	size_t dummy2;
	size_t dummy3;
	unsigned int dummy4;
} StaticMemPool_t;

/* StaticMemPool_t
 * Static block header, can be used for exact static block buffer preallocation.
 * Each block takes ItemSize + sizeof(StaticMemPool_t) bytes.
 */
typedef struct {
	void *dummy;
} StaticMemItem_t;

/* MemPoolCreate
 * Creates a new pool instance on heap.
 * @param ItemSize Block size, must be > 0
 * @param ItemCount Block count, must be > 0
 * @return MemPool_t Pool handle or NULL on fail
 */
MemPool_t MemPoolCreate(size_t ItemSize, size_t ItemCount);

/* MemPoolCreateStatic
 * Creates a new pool instance using provided static buffer and buffer.
 * @param ItemSize Block size, must be > 0
 * @param ItemCount Block count, 0 is used for auto-calculation to fit BufferSize
 * @param Buffer Pointer to static block buffer
 * @param BufferSize Static block buffer size
 * @param MemPoolBuffer Static pool buffer
 * @return MemPool_t Pool handle or NULL on fail
 */
MemPool_t MemPoolCreateStatic(size_t ItemSize, size_t ItemCount, void *Buffer, size_t BufferSize, StaticMemPool_t *MemPoolBuffer);

/* MemPoolDelete
 * Deletes pool instance, frees memory if it was allocated.
 * @param pool Pool handle, NULL value is ignored
 */
void MemPoolDelete(MemPool_t pool);

/* MemPoolAlloc
 * Allocates a new block from pool instance.
 * @param pool Pool handle, must be valid
 * @return Pointer to allocated block or NULL on fail
 */
void *MemPoolAlloc(MemPool_t pool);

/* MemPoolFree
 * Returns previously allocated block to the pool.
 * @param pool Pool handle, must be valid
 * @param ptr Pointer to allocated block, NULL value is ignored
 */
void MemPoolFree(MemPool_t pool, void *ptr);

/* MemPoolFreePtr
 * Returns previously allocated block to its pool.
 * @param ptr Pointer to allocated block, NULL value is ignored
 */
void MemPoolFreePtr(void *ptr);

/* MemPoolAvailable
 * Allows to get a number of free blocks available in the pool.
 * @param pool Pool handle, NULL value is ignored
 * @return size_t free block count
 */
size_t MemPoolAvailable(MemPool_t pool);

#endif
