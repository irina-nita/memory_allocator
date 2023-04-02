/** @file seg_malloc.h
 */

#ifndef SEG_MALLOC_H
#define SEG_MALLOC_H

#include <unistd.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#define ALIGNMENT 8 /* 8-byte alignment */
#define ALIGN(x) (((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1)) /* Align to 8-byte alignment */

#define HEADER_SIZE (ALIGN(sizeof(Header)))
#define FOOTER_SIZE (ALIGN(sizeof(Footer)))

#define MIN_SIZE 16
#define MIN_SIZE_BLOCK (HEADER_SIZE + FOOTER_SIZE)
#define MAX_SIZE 1024
#define MAX_SIZE_BLOCK (HEADER_SIZE + FOOTER_SIZE + MAX_SIZE)

#define PAGE_SIZE ((uint64_t)sysconf(_SC_PAGESIZE)) /* System page size */

#define MIN_MAP_SIZE ((uint64_t)1025) /* Minimum size for calling mmap */
#define HEAP_SIZE(size) (ALLOC_SIZE(size) < MIN_MAP_SIZE)

#define ALLOC_SIZE(x) (((x) < 16) ? 16 : ALIGN(x)) /* 8-byte align and assure minimum 16 bytes. */

/* Node size including payload, header and footer. Fit for free & allocated (on heap) nodes. */
#define HEAP_INC(x) (ALLOC_SIZE(x) + sizeof(uint64_t) + FOOTER_SIZE)

/** Node size including payload and header.
 * Fit for free & allocated (on memory mapped region) nodes.
 */
#define MAP_INC(x) (ALLOC_SIZE(x) + sizeof(uint64_t))

#define BUCKET_INDEX(x) (_bucket_index(x))

#define SPLIT_SIZE(full_size, size) (((full_size) - FOOTER_SIZE - ALIGN(sizeof(uint64_t))) - ALLOC_SIZE(size))
#define WORTH_SPLIT(full_size, size) ((full_size) >= (MIN_SIZE_BLOCK + ALLOC_SIZE(size)))
/* defines for buckets */
#define NUM_BUCKETS 8 /* {16, 32, 64, 128, 256, 512, 1024, 1025-inf} */

#define BUCKET_16(x) ((x) <= 16)
#define BUCKET_PAGE(x) ((x) > 1024)

#define BUCKET(x, l, r) ((x) > (l) && x <= (r))

#define MEMCPY(destination, source, num) (_memcpy((destination), (source), (num)))

/* Number of memory pages for size */
#define PAGES(x) ((ceil)((float)(x) / PAGE_SIZE))

typedef struct header Header; /* Header for free nodes */
typedef uint64_t Footer; /* Footer for heap nodes */

struct header {
  uint64_t flags;
  Header* next; /* next node in free list */
  Header* prev; /* previous node in free list */
};

extern void* heap_start; /* Pointer to the start of heap */

extern void* heap_end; /* Pointer to the end of heap */

extern Header* buckets[NUM_BUCKETS]; /* Array of pointers to the head of each bucket */

/* Array of maximum sizes for buckets */
extern uint64_t sizes[NUM_BUCKETS];

/**
 * @param size       Size of the free node
 * @return           Index of minimum bucket to fit size
 */
static int
_bucket_index(uint32_t size)
{
    if (size <= MIN_SIZE)
        return 0;
    if (size > MAX_SIZE)
        return NUM_BUCKETS - 1;
    uint32_t bucket = MIN_SIZE;
    int idx = 1;
    while (bucket < MAX_SIZE) {
        if (size > bucket && size < (bucket << 1))
            return idx;
        idx++;
        bucket <<= 1;
    }
}

/**
 * @brief            Simple implementation of memcpy
 */
static void
_memcpy(void *destination, const void *source, size_t num)
{
	char* dp = (char *)destination;
	char* sp = (char *)source;
	unsigned long i = 0;

	while (i < num) {
		*dp = *sp;
		dp++;
		sp++;
		i++;
	}
}

/**
 * @brief            Get a chunk of memory by calling sbrk/mmap
 * @param size       Size of the chunk
 * @return           Pointer to the start of the chunk
*/
void* chunk_get(uint64_t size) __attribute__ ((visibility ("internal")));

/**
 * @brief            Creates a new free node with header and footer
 * @param size       Size of the node
 * @return           Pointer to the header of the free node
*/
Header* blk_create(uint64_t size) __attribute__ ((visibility ("internal")));

/**
 * @brief            Insert a free block as head to bucket
 * @param free_blk   Pointer to the header of the free block
 * @return           Void
*/
void blk_insert(Header* free_blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Removes a free block from bucket
 * @param free_blk   Pointer to the header of the free block
 * @return           Void
*/
void blk_remove(Header *blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Finds the first free node of given minimum size in bucket
 * @param bucket_idx Index of the bucket to search in
 * @param size       Minimum size for the node
 * @return           Pointer to the header of the free node
*/
Header* first_fit(uint32_t bucket_idx, uint64_t size) __attribute__ ((visibility ("internal")));

/**
 * @brief            Finds the first free node of given minimum size
 * @param size       Minimum size for the node
 * @return           Pointer to the header of the free node
*/
Header* search_free_blk(uint64_t size) __attribute__ ((visibility ("internal")));

/**
 * @brief            Gets a free node of given minimum size by either finding it in the free list
 *                   or creating a new one.
 * @param size       Minimum size for the node
 * @return           Pointer to the header of the free node
*/
Header* get_free_blk(uint64_t size) __attribute__ ((visibility ("internal")));

/**
 * @brief            Creates a new free node and adds it to bucket
 * @param size       Size required for the free node.
 * @return           Pointer to the header of the free node              
*/
Header* new_free_blk(uint64_t size) __attribute__ ((visibility ("internal")));

/**
 * @brief            Check if the previous node in terms of address is free or allocated.
 * @param free_blk   Current free node
 * @return           8-bit integer as 0 or 1
*/
uint8_t check_prev_free(Header *free_blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Check if the next node in terms of address is free or allocated.
 * @param free_blk   Current free node
 * @return           8-bit integer as 0 or 1
*/
uint8_t check_next_free(Header *free_blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Mark a node as allocated by changing the header and footer
 * @param free_blk   Current free node
 * @return           Void
*/
void mark_alloc(Header *free_blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Mark a node as free by changing the header and footer
 * @param alloc_blk  Current allocated node
 * @return           Void
*/
void mark_dealloc(Header* alloc_blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Split a free node in an allocated node
 *                   of given size and a free node of the remaining size
 * @param free_blk   Free node to split
 * @param size       Size needed to allocate
 * @return           Void
 * @attention        This function assumes the size is appropriate for splitting
*/
void split(Header *free_blk, uint64_t size) __attribute__ ((visibility ("internal")));

/**
 * @brief            Coalesce the free node with previous/next (in terms of address) free node
 * @param free_blk   Free node
 * @return           Pointer to the header of the coalesced node
*/
Header* coalesce(Header *free_blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Coalesce the free node with next (in terms of address) free node
 * @param free_blk   Free node
 * @return           Pointer to the header of the coalesced node
*/
Header* coalesce_next(Header *free_blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Coalesce the free node with previous (in terms of address) free node
 * @param free_blk   Free node
 * @return           Pointer to the header of the coalesced node
*/
Header* coalesce_prev(Header* free_blk) __attribute__ ((visibility ("internal")));

/**
 * @brief            Malloc
 * @param size       Size to be allocated
 * @return           Pointer value that can later be successfully passed to seg_free()
*/
void *seg_malloc(size_t size) __attribute__ ((malloc));

/**
 * @brief            Calloc
 * @param nmemb      Number of elements
 * @param size       Size of element
 * @return           Pointer value that can later be successfully passed to seg_free()
*/
void *seg_calloc(size_t nmemb, size_t size) __attribute__ ((malloc));

/**
 * @brief            Realloc
 * @param ptr        Pointer of allocated memory
 * @param size       New size for the allocated memory
 * @return           Pointer value that can later be successfully passed to seg_free()
*/
void *seg_realloc(void *ptr, size_t size) __attribute__ ((malloc));

/**
 * @brief            Free
 * @param            Pointer to the memory to be freed
 * @return           Void
*/
void seg_free(void *ptr);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SEG_MALLOC_H */
