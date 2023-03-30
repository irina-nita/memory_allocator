/**
 * \file            seg_malloc.c
 * \brief           Segregated Malloc source file
 */

#include "seg_malloc.h"
#include <assert.h>
#include <stdint.h>

Header* buckets[NUM_BUCKETS] = { [0 ... (NUM_BUCKETS - 1)] = NULL };

/**
 * returns bucket for given size
*/

uint64_t
bucket(uint64_t size) {

  if (BUCKET_16(size)) {
    return MIN_SIZE;
  }

  for (uint64_t bkt = 8; bkt < MAX_SIZE; bkt <<= 2) {
    if (BUCKET(size, bkt, bkt << 2)) { /* verif. align */
      return bkt << 2;
    }
  }

  if (BUCKET_PAGE(size)) {
    return PAGE_SIZE;  
  }
  return -1;
}

/**
 * inserts new block in the corresponfing bucket
 * new_blk == NULL => previous search didn't find a free block for the given size => inc. heap break/ memory map
*/

Header*
blk_insert(uint64_t size, Header* new_blk)
{
  /* get bucket for given size */
  uint64_t bkt = bucket(size);
  assert(bkt > 0);
  
  /* previous search didn't find a free block for the given size => inc. heap break/ memory map */
  if (new_blk == NULL) {
      
    new_blk = (bkt < PAGE_SIZE) ?
      sbrk(HEAP_INC(size)) :
      mmap(NULL, (size_t)PAGES(size), PROT_READ | PROT_WRITE, MAP_ANON, -1, 0);
      assert(new_blk != NULL);
    printf("YOU'RE ON THE RIGHT TRACK, KEEP GOING....\n");
    printf("YOUR MEMORY ADDRESS FOR FREE BLOCK IS: %p\n", new_blk);
  }

  /* insert new_blk as head of bucket */
  if (buckets[BUCKET_INDEX(bkt)] == NULL) {

    new_blk->next = new_blk;
    new_blk->prev = new_blk; 
    new_blk->flags = ALLOC_SIZE(size);

    printf("YOUR HEADER IS AT ADDRESS: %p AND THE FLAGS ARE %lu\n", &(new_blk->flags), new_blk->flags);

    /* add the footer */
    Footer* footer = (Footer* )((char* )new_blk + HEAP_INC(size) - FOOTER_SIZE);
    *footer = *footer ^ *footer;
    *footer = (Footer) ALLOC_SIZE(size);
    
    printf("YOUR FOOTER IS AT ADDRESS %p AND THE FLAGS ARE %lu\n", footer, *footer);

    buckets[BUCKET_INDEX(bkt)] = new_blk;

    return new_blk;

  } else {
    
    new_blk->flags = ALLOC_SIZE(size);
    new_blk->prev = new_blk;
    new_blk->next = buckets[BUCKET_INDEX(bkt)];
    buckets[BUCKET_INDEX(bkt)]->prev = new_blk;

    /* add the footer */
    *(Footer *)((char *)new_blk + HEAP_INC(size) - FOOTER_SIZE) = (Footer) ALIGN(size);

    buckets[BUCKET_INDEX(bkt)] = new_blk;

    return new_blk;

  }
}

/*
 * remove free block from bucket
 */

void
blk_remove(Header* free_blk)
{
  /* get bucket for given size */ 
  uint64_t size = free_blk->flags & ~1;
  uint64_t bkt = bucket(size);
  
  printf("NOW REMOVING FROM THE FREE LIST SO WE KNOW IT'S ALLOCATED...");

  if (free_blk == buckets[BUCKET_INDEX(bkt)]) {
    /* it's head */
    buckets[BUCKET_INDEX(bkt)] = (free_blk->next != free_blk) ? free_blk->next : NULL;
    printf("%p SHOULD BE NULL\n", buckets[BUCKET_INDEX(bkt)]);
    printf("BUT YOU SHOULDN'T: %p\n", free_blk);
    free_blk->next = 0x00;
    free_blk->prev = 0x00;
    return;
  }
  
  free_blk->next->prev = free_blk->prev;
  free_blk->prev->next = free_blk->next;
  free_blk->next = 0x00;
  free_blk->prev = 0x00;

}

/* mark block as allocated and rearrange header */
//TODO: make defines and change the way blocks and pointers are sized!!!

/** free block
| 24 bytes                           | aligned to 8 bytes                        | 8 bytes  |
| ---------------------------------- | ----------------------------------------- | -------- |
| free block header                  | payload               | padding           | footer   |
| ---------------------------------- | ----------------------------------------  | ---------|
*/

/** allocated block
| 8 bytes                | aligned to 8 bytes                                    | 8 bytes  |
| ---------------------- | ----------------------------------------------------- | -------- |
| allocated block header | payload                             | padding         | footer   |
| ---------- ----------- | ----------------------------------------------------- | ---------|
*/

void
blk_alloc(Header* fb, uint64_t size)
{
  /* remember old size */
  uint64_t old_size = fb->flags & ~1;
  
  /* mark as allocated and update the size of the allocation */
  fb->flags = ALLOC_SIZE(size) | 1;

  printf("ALLOCATING THE BLOCK FOR YOU RN....\n");
  printf("YOU SHOULD HAVE A BLOCK WITH FLAGS AT %p HAVING %lu AS VALUE\n", &(fb->flags), fb->flags);

  /* update footer */
  *(Footer *)((char *)fb + HEAP_INC(size) - FOOTER_SIZE) = (Footer) fb->flags;
  
  /* remove from free list */
  blk_remove(fb);
  
  /* todo: return Header* */

  /* split if necessary */
  printf("The size I'm calling this with might be wrong: %lu\n", old_size);
  split(fb, old_size);
}

Header*
blk_dealloc(Header* ab)
{
  /* mark as free */
  ab->flags &= ~1;
  ab = blk_insert(ab->flags, ab);
  return coalesce(ab);
}

/*
 * \uint64_t size   minimum size of the free block needed
 * \Header*         free block found in free list
 */

Header*
search_fb(uint64_t size)
{
  /* get size of bucket */
  uint64_t bkt = bucket(size);
  assert(bkt > 0);
 
  /* 551 */
  printf("YOUR BUCKET SIZE IS: %lu\n", bkt);

  while (bkt <= MAX_SIZE) {
    /* get list head */
    Header* head = buckets[BUCKET_INDEX(bkt)];
    
    if (head == NULL) {
      bkt <<=2;
      continue;
    }

    Header* ptr = head;
    /* search for free block */ //todo
    for (; ptr->next != head; ptr = ptr->next) {
      if (ALLOC_SIZE(size) < (ptr->flags & ~1)) {
        return ptr; /* will need to blk_alloc(this) */
      }
    }
    bkt <<= 2;
  }

  /* if not found in free list, ask from the OS for more memory */
  return blk_insert(size, NULL);
}

/*
 * \Header* fb     a free memory block
 * \return         if the previous block is free or allocated
 */

uint8_t
check_prev_free(Header* fb)
{
  Footer prev_footer = *(Footer*)((char*)fb - FOOTER_SIZE);
  if (prev_footer & 1) {
    return 0;
  }
  return 1;
}

/*
 * \Header* fb     a free memory block
 * \return         if the next block is free or allocated
 */

uint8_t
check_next_free(Header* fb)
{
  uint64_t size = fb->flags & ~1;
  uint64_t next_header_flags = *(uint64_t*)((char*)fb + ALIGN(size) +  ALIGN(sizeof(Footer)));

  if (next_header_flags & 1) {
    return 0;
  }

  return 1;
}

/*
 * \Header* ab      the free block just allocated
 * \uint64_t size   the size needed to be allocated to that block
 */

void
split(Header* ab, uint64_t size)
{
  /* 1. check if it has the right size left
   * (ALIGN THE SIZE TO 8 BYTES) to split */
  
  /* 2. if the ALIGN(size) is less then the size
   * todo: we check coalescing_right // rn not doing that, stopping here
   * */
  
  printf("SIZE: %lu\n", size);
  printf("MIN_SIZE_BLOCK: %lu\n", MIN_SIZE_BLOCK);

  int remaining = size - MIN_SIZE_BLOCK;

  printf("SOMEHOW I GOT HERE with an overflow ig: %d\n", remaining);
  if (remaining < 0) {
    return; // NULL
  }

  uint64_t rem_size = (uint64_t)rem_size;

  printf("SOMEHOW I GOT HERE\n");
 
  /* 3. if the size is fit, we mark it as a new block and add it to the free list 
   * */

  // 3.1 add footer to the ab section
  Footer* new_footer = (Footer*)((char*)ab + ALIGN(size) + sizeof(Header));
  *new_footer = ALLOC_SIZE(size);

  // 3.2 add Header for the new block
  Header* header = (Header* )((char*)new_footer + ALIGN(sizeof(Footer)));
  header->flags = ALLOC_SIZE(rem_size);

  // 3.3 update footer
  Footer* old_footer = (Footer*)((char*)ab +  ALIGN(ab->flags & ~1) + ALIGN(sizeof(Header)));
  *old_footer = ALLOC_SIZE(rem_size);

  // 3.4 Add block
  blk_insert(rem_size, header);
}

Header*
coalesce(Header* fb)
{
  uint8_t next_free = check_next_free(fb);
  uint8_t prev_free = check_prev_free(fb);

  /* 1-0 second case */
  if (next_free && !prev_free) {
    return coalesce_next(fb);
  }
  
  /* 0-1 third case */
  if (!next_free && prev_free) {
    return coalesce_prev(fb);
  }

  /* 1-1 fourth case */
  if (next_free && prev_free) {
    return coalesce_next(coalesce_prev(fb));
  }

    /* 0-0 first case */
    return NULL;
}

Header*
coalesce_next(Header* fb)
{
  Header* next_blk = (Header*)((char* )fb + ALIGN(sizeof(Header))
                    + ALIGN(fb->flags & ~1)
                    + ALIGN(sizeof(Footer)));

  Footer* next_footer = (Footer*)((char*)next_blk
                      + ALIGN(next_blk->flags & ~1)
                      + ALIGN(sizeof(Header)));

  uint64_t new_size = ALIGN(sizeof(Header) + sizeof(Footer))
                      + (fb->flags & ~1) + (next_blk->flags & ~1);
  fb->flags = new_size;
  *next_footer = new_size;

  blk_remove(next_blk);
  blk_remove(fb);
  blk_insert(new_size, fb);
  return fb;
}


Header*
coalesce_prev(Header* fb)
{
  uint64_t prev_blk_size = *(Footer*) ((char*)fb - ALIGN(sizeof(Footer))) & ~1;
  Header* prev_blk = (Header*) ((char*)fb
                      - ALIGN(sizeof(Header))
                      - ALIGN(prev_blk_size)
                      - ALIGN(sizeof(Footer)));
  uint64_t new_size = ALIGN(sizeof(Header))
                              + ALIGN(sizeof(Footer))
                              + ALIGN(fb->flags & ~1)
                              + ALIGN(prev_blk_size);

  Header* new_header = prev_blk;
  new_header->flags = new_size;

  blk_remove(prev_blk);
  blk_remove(fb);
  blk_insert(new_size, new_header);
  return new_header;
}

void*
seg_malloc(size_t size)
{
  assert(size > 0);
  Header* header = search_fb(size);

  printf("I'M IN MALLOC. THE HEADER OF THE BLK IS: %p\n", header);
  
  void* ptr = (void*)((char*)header + ALIGN(sizeof(uint64_t)));

  printf("I'M IN MALLOC. THE ADDRESS YOU GET SHOULD BE: %p\n", ptr);
  
  blk_alloc(header, size);

  return ptr;
}

void
seg_free(void *ptr)
{
  assert(ptr != NULL);
  Header* header = (Header*)((char*)ptr - ALIGN(sizeof(Header)));
  blk_dealloc(header);
}
