/**
 * \file            seg_malloc.c
 * \brief           Segregated Malloc source file
 */

#include "seg_malloc.h"
#include <assert.h>
#include <stdint.h>

Header* buckets[NUM_BUCKETS] = { [0 ... (NUM_BUCKETS - 1)] = NULL };

/*
 * initialize memory by adding the heads of the buckets
 */
void
mem_init()
{
  //todo: find something meaningful to do here
}

uint64_t
bucket(uint64_t size) {

  if (BUCKET_8(size)) {
    return MIN_SIZE;
  }

  for (uint64_t bkt = 8; bkt < MAX_SIZE; bkt << 2) {
    if (BUCKET(size, bkt, bkt << 2)) {
      return bkt << 2;
    }
  }

  if (BUCKET_PAGE(size)) {
    return PAGE_SIZE;  
  }
  return -1;
}

void
blk_insert(uint64_t size, Header* fb)
{
  /* get size of bucket */
  uint64_t bkt = bucket(size);
  assert(bkt > 0);

  /* get the approppriate bucket*/
  Header* head = buckets[BUCKET_INDEX(bkt)];
  
  if (head == NULL) {
    
    head = (bkt < PAGE_SIZE) ?
      sbrk(HEAP_INC(size)) :
      mmap(NULL, (size_t)PAGES(size), PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
    
    head->next = head;
    head->prev = head;
    head->flags = ALIGN(size); /* it's free */
    
    /* add the footer */
    *(Footer *)((char *)head + HEAP_INC(size) - FOOTER_SIZE) = (Footer) ALIGN(size);

  } else {
    
    Header* fb = (bkt < PAGE_SIZE) ?
      sbrk(HEAP_INC(size)) :
      mmap(NULL, (size_t)PAGES(size), PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
    
    fb->flags = ALIGN(size);
    fb->prev = head;
    fb->next = head->next;
    head->next->prev = fb;
    head->next = fb;

    /* add the footer */
    *(Footer *)((char *)head + HEAP_INC(size) - FOOTER_SIZE) = (Footer) ALIGN(size);
  }
}

void
blk_remove(Header *fb)
{
  fb->prev->next = fb->next;
  fb->next->prev = fb->prev;
  fb->next = NULL;
  fb->prev = NULL;
}

void
blk_alloc(Header* fb, uint64_t size)
{
  /* remember old size */
  uint64_t old_size = fb->flags & ~1;
  
  /* mark as allocated and update the size of the allocation */
  fb->flags = ALIGN(size) | 1;

  /* update footer */
  *(Footer *)((char *)head + HEAP_INC(size) - FOOTER_SIZE) = (Footer) fb->flags;
  
  /* remove from free list */
  blk_remove(fb);
  
  /* split if necessary */
  split(fb, old_size);
}



