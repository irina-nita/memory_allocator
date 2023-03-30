/**
 * \file            seg_malloc.h
 * \brief           Segregated Malloc include file
 */

#ifndef SEG_MALLOC_H
#define SEG_MALLOC_H

#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <math.h>
#include <sys/mman.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* defines for size */
#define ALIGNMENT 8
#define ALIGN(x) (((x) + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1))

#define MIN_SIZE 16
#define MIN_SIZE_BLOCK (ALIGN((sizeof(Header)) + (sizeof(Footer)) + (MIN_SIZE)))

#define MAX_SIZE 1024
#define MAX_SIZE_BLOCK (ALIGN(sizeof(Header)) + (sizeof(Footer)) + (MAX_SIZE))

#define PAGE_SIZE ((uint64_t)sysconf(_SC_PAGESIZE))

#define HEADER_SIZE (ALIGN(sizeof(Header)))
#define FOOTER_SIZE (ALIGN(sizeof(Footer)))

#define ALLOC_SIZE(x) (((x) < 16) ? 16 : ALIGN(x))
#define HEAP_INC(x) (ALLOC_SIZE(x) + HEADER_SIZE + FOOTER_SIZE - (16))

/* defines for buckets */
#define NUM_BUCKETS 8 /* {16, 32, 64, 128, 256, 512, 1024, 1025-inf} */
#define BUCKET_INDEX(x) ((ALLOC_SIZE(x) != PAGE_SIZE) ? (uint64_t)(log2((x) >> 3)) : 8)

#define BUCKET_16(x) ((x) <= 16)
#define BUCKET_PAGE(x) ((x) > 1024) /* -> 4096 */

#define BUCKET(x, l, r) ((x) > (l) && x <= (r))

#define PAGES(x) ((ceil)((x) / PAGE_SIZE) + 1)

/** free block header
23               14               7             0
| --------------- | ------------- | ----------- |
| header.flags    | header.next   | header.prev |
|(size      | f/a)|               |             |
| --------------- | ------------- | ----------- |
*/

typedef struct header Header;

struct header {
  uint64_t flags; /* size & last two bits for flags */
  Header* next;
  Header* prev;
};

/** allocated block header
7            0
| flags      |
|(size | f/a)|
| ---------- |
*/

/** footer
7            0
| flags      |
|(size | f/a)|
| ---------- |
*/

typedef size_t Footer;

/**
| buckets[0] | ------> | free block <= 8 bytes | ------> | free block <= 8 bytes |
|            | <------ |                       | <------ |                       |
*/

extern Header* buckets[NUM_BUCKETS];

uint64_t bucket(uint64_t size);

Header*  blk_insert(uint64_t size, Header* new_blk);
void     blk_remove(Header* fb);
void     blk_alloc(Header* fb, uint64_t size);
Header*  blk_dealloc(Header* ab);

void*    seg_malloc(size_t size);
void     seg_free(void* ptr);

Header*  coalesce(Header* fb);
Header*  coalesce_next(Header* fb);
Header*  coalesce_prev(Header* fb);
void     split(Header* ab, uint64_t size);

Header*  search_fb(uint64_t size);

uint8_t  check_next_free(Header* fb);
uint8_t  check_prev_free(Header* fb);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SEG_MALLOC_H */

