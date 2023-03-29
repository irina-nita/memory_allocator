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

typedef struct header Header;
typedef size_t Footer;

struct header {
  uint64_t flags; /* size & last two bits for flags */
  Header* next;
  Header* prev;
};

#define ALIGNMENT 8
#define ALIGN(x) ((x) & ~(ALIGNMENT - 1))

#define MIN_SIZE 8
#define MIN_SIZE_BLOCK (ALIGN((sizeof(Header)) + (sizeof(Footer)) + (MIN_SIZE)))

#define MAX_SIZE 1024
#define MAX_SIZE_BLOCK (ALIGN(sizeof(Header)) + (sizeof(Footer)) + (MAX_SIZE))

#define PAGE_SIZE ((uint64_t)sysconf(_SC_PAGESIZE))

#define BUCKET_INDEX(x) (((x) != PAGE_SIZE) ? (uint64_t)(log2((x) >> 3)) : 8)
#define HEADER_SIZE (ALIGN(sizeof(Header)))
#define FOOTER_SIZE (ALIGN(sizeof(Footer)))

#define HEAP_INC(x) (ALIGN(x) + HEADER_SIZE + FOOTER_SIZE)

#define NUM_BUCKETS 9 /* {8, 16, 32, 64, 128, 256, 512, 1024, 1025-inf} */

extern Header* buckets[NUM_BUCKETS];

/* checks for bucket correspondence */
#define BUCKET_8(x) ((x) <= 8)
#define BUCKET_16(x) ((x) > 8 && (x) <= 16)
#define BUCKET_32(x) ((x) > 16 && (x) <= 32)
#define BUCKET_64(x) ((x) > 32 && (x) <= 64)
#define BUCKET_128(x) ((x) > 32 && (x) <= 128)
#define BUCKET_512(x) ((x) > 128 && (x) <= 512)
#define BUCKET_1024(x) ((x) > 512 && (x) <=1024)
#define BUCKET_PAGE(x) ((x) > 1024) /* -> 4096 */

#define BUCKET(x, l, r) ((x) > (l) && x <= (r))

#define PAGES(x) ((ceil)((x) / PAGE_SIZE) + 1)
void     mem_init();

uint64_t bucket(uint64_t size);

void     blk_insert(uint64_t size, Header* fb);
void     blk_remove(Header* fb);
void     blk_alloc(Header* fb, uint64_t size);
Header*  blk_dealloc(Header* ab);

void*    seg_malloc(size_t size);
void     seg_free(void* ptr);

Header*  coalesce(Header* fb);
void     split(Header* ab, uint64_t size);

Header*  search_fb(uint64_t size);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* SEG_MALLOC_H */

