/** @file segmalloc.c
 * 
 * Simple implementation of malloc, realloc, calloc and free using the segregated free list
 * approach (an array of free lists). Each array contains blocks of the same class size (power of two).
 * 
 * The "Small" classes have sizes ranging from 16 to 1024 bytes. They are allocated using the sbrk system call
 * and can be split and coalesced upon allocating/deallocating to prevent fragmentation.
 * 
 * The "Big" class has sizes larger than 1024 bytes. They are allocated/deallocated using mmap/munmap system calls
 * and cannot be split or coalesced.
 * Note: Upon coalescing, there can be formed blocks belonging to the "big" class size.
 * These blocks can be split/coalesced. The tracking of the type of allocation is kept in
 * the 'flags' member of the header struct.
 * 
 * free block
 * 		| --------------------- | ------------------------------------------ | --------|
 * 		| 8 bytes               | 8 bytes | 8 bytes | aligned to 8 bytes     | 8 bytes |
 * 		| --------------------- | ----------------- | -----------------------|         |
 * 		| size         |m/h||a/f| *prev  | *next    | size_of_payload - 16 ||| footer  |
 * 		| --------------------- | ------------------------------------------ | --------|
 * 
 * allocated block
 * 		| --------------------- | ------------------------------------------ | --------|
 * 		| 8 bytes               | aligned to 8 bytes                         | 8 bytes |
 * 		| --------------------- | ----------------- | -----------------------|         |
 * 		| size         |m/h||a/f| payload                       | padding |||| footer  |
 * 		| --------------------- | ------------------------------------------ | --------|
 * 
 * Note: There is no use in keeping track of next/previous node pointers in allocated blocks,
 * so they get overwritten. (The reason why the minimum size for a block is 16 bytes).
 * 
 * Note: The blocks with m flag set as 1 (mmap-ed) have no footer, since there they can't be split
 * nor coalesced.
 */
#include "segmalloc.h"

Header *buckets[NUM_BUCKETS] = {[0 ...(NUM_BUCKETS - 1)] = NULL};
uint64_t sizes[NUM_BUCKETS] = {16, 32, 64, 128, 256, 512, 1024, INT32_MAX};
void *heap_start = NULL;
void *heap_end = NULL;

void*
chunk_get(uint64_t size)
{
	/* get chunk from OS */
	Header *new_blk = (HEAP_SIZE(size))
					? sbrk(HEAP_INC(size))
					: mmap(NULL, (size_t)PAGES(MAP_INC(size)) * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);

	/* check if it returned a chunk */
	assert(new_blk != NULL);

	/* update heap_start and heap_end */
	if (heap_start == NULL && HEAP_SIZE(size))
		heap_start = new_blk;

	if (HEAP_SIZE(size))
		heap_end = (char *)new_blk + HEAP_INC(size);

	return new_blk;
}

Header*
blk_create(uint64_t size)
{
	Header *new_blk = (Header*)chunk_get(size);

	/* add flag for memory region */
	new_blk->flags = (HEAP_SIZE(size)) ? (ALLOC_SIZE(size)) : (ALLOC_SIZE(size) | 2);

	/* add footer if necessary */
	if (HEAP_SIZE(size))
		*(Footer *)((char *)new_blk + HEAP_INC(size) - FOOTER_SIZE) = (Footer)new_blk->flags;

	return new_blk;
}

void
blk_insert(Header *free_blk)
{
	/* insert new_blk as head of bucket */
	if (buckets[BUCKET_INDEX(free_blk->flags)] == NULL)
	{
		free_blk->next = NULL;
		free_blk->prev = NULL;

		buckets[BUCKET_INDEX(free_blk->flags)] = free_blk;
	}
	else
	{
		buckets[BUCKET_INDEX(free_blk->flags)]->prev = free_blk;
		free_blk->next = buckets[BUCKET_INDEX(free_blk->flags)];
		free_blk->prev = NULL;

		buckets[BUCKET_INDEX(free_blk->flags)] = free_blk;
	}
}

void
blk_remove(Header *free_blk)
{
	if (free_blk == buckets[BUCKET_INDEX(free_blk->flags & ~1)])
	{
		buckets[BUCKET_INDEX(free_blk->flags)] = free_blk->next;
		if (buckets[BUCKET_INDEX(free_blk->flags)] != NULL)
			if (buckets[BUCKET_INDEX(free_blk->flags)]->prev != NULL)
		 		buckets[BUCKET_INDEX(free_blk->flags)]->prev = NULL;
	}
	else
	{
		if (free_blk->next == NULL) { /* last node */
			free_blk->prev->next = NULL;
		} else {
			free_blk->next->prev = free_blk->prev;
			free_blk->prev->next = free_blk->next;
		}
	}
}

Header*
first_fit(uint32_t bucket_idx, uint64_t size)
{
	Header* head = buckets[bucket_idx];
	Header* ptr = head;
	for(; ptr != NULL; ptr = ptr->next) {
		if (size < ptr->flags) {
			return ptr;
		}
	}
	return NULL;
}

Header*
search_free_blk(uint64_t size)
{
	Header* free_blk;
	uint32_t bucket_idx = BUCKET_INDEX(size);
	while(bucket_idx < NUM_BUCKETS) {
		if ((free_blk = first_fit(bucket_idx, size)) != NULL) {
			return free_blk;
		}
		bucket_idx++;
	}
	return NULL;
}

Header*
get_free_blk(uint64_t size)
{
	Header *free_blk = search_free_blk(size);
	if (free_blk != NULL) { /*if free node exists*/
		if (WORTH_SPLIT(free_blk->flags, size) && !((free_blk->flags) & 2))
			split(free_blk, size);
		return free_blk;
	}
	return new_free_blk(size);
}

Header*
new_free_blk(uint64_t size)
{
	Header *free_blk = blk_create(size);
	assert(free_blk != NULL);

	blk_insert(free_blk);

	return free_blk;
}

uint8_t
check_prev_free(Header *free_blk)
{
	if (heap_start == free_blk)
	{
		return 0;
	}
	Footer prev_footer = *(Footer *)((char *)free_blk - FOOTER_SIZE);
	if (prev_footer & 1)
	{
		return 0;
	}
	return 1;
}

uint8_t
check_next_free(Header *free_blk)
{
	uint64_t size = free_blk->flags & ~1;
	void *next_header = (char *)free_blk + ALIGN(sizeof(uint64_t)) + ALIGN(size) + ALIGN(sizeof(Footer));

	if (next_header == heap_end)
		return 0;

	uint64_t next_header_flags = *(uint64_t *)(next_header);

	if (next_header_flags & 1)
		return 0;

	return 1;
}

void
mark_alloc(Header *free_blk)
{
	uint64_t size = free_blk->flags;

	free_blk->flags |= 1;

	Footer *footer = (Footer *)((char *)free_blk + HEAP_INC(size) - FOOTER_SIZE);
	*footer = (Footer)(free_blk->flags);
}

void
mark_dealloc(Header *alloc_blk)
{
	uint64_t size = alloc_blk->flags & ~3;

	alloc_blk->flags &= ~1;

	if (!(alloc_blk->flags & 2)) {
		Footer *footer = (Footer *)((char *)alloc_blk + HEAP_INC(size) - FOOTER_SIZE);
		*footer = (Footer)(alloc_blk->flags);
	}

	alloc_blk->next = NULL;
	alloc_blk->prev = NULL;
}

void
split(Header *free_blk, uint64_t size)
{
	uint64_t full_size = free_blk->flags;
	
	blk_remove(free_blk); /*remove old node*/

	uint64_t right_split_size = SPLIT_SIZE(full_size, size);
	uint64_t left_split_size = ALLOC_SIZE(size);

	/*left split footer and header */
	Header *left_split_header = free_blk;
	left_split_header->flags = left_split_size;

	Footer *left_split_footer = (Footer *)((char *)free_blk + left_split_size + sizeof(uint64_t));
	*left_split_footer = left_split_size;

	/*right split header and footer*/
	Header *right_split_header = (Header *)((char *)left_split_footer + FOOTER_SIZE);
	right_split_header->flags = right_split_size;

	Footer *right_split_footer = (Footer *)((char *)right_split_header + HEAP_INC(right_split_size) - FOOTER_SIZE);

	*right_split_footer = right_split_size;

	/*insert new nodes as a free*/
	blk_insert(left_split_header);
	blk_insert(right_split_header);
}

Header*
coalesce(Header *free_blk)
{
	uint8_t _next_free = check_next_free(free_blk);
	uint8_t _prev_free = check_prev_free(free_blk);

	if (_next_free && !_prev_free)
		return coalesce_next(free_blk);

	if (!_next_free && _prev_free)
		return coalesce_prev(free_blk);

	if (_next_free && _prev_free)
		return coalesce_next(coalesce_prev(free_blk));

	return free_blk;
}

Header*
coalesce_next(Header *free_blk)
{	
	Header *next_blk = (Header *)((char *)free_blk + HEAP_INC(free_blk->flags)); /*next node header*/

	Header *full_blk = free_blk; /*new coalesced node header*/

	uint64_t full_size = next_blk->flags
						+ full_blk->flags
						+ ALIGN(sizeof(uint64_t))
						+ ALIGN(sizeof(Footer)); /*size for full node*/

	blk_remove(next_blk); /*remove next node from list*/

	/*change header & footer size*/
	full_blk->flags = full_size;
	Footer* full_footer = (Footer *)((char *)full_blk + HEAP_INC(full_size) - FOOTER_SIZE);
	*full_footer = full_size;

	return full_blk;
}

Header*
coalesce_prev(Header *free_blk)
{
	Footer *prev_footer = (Footer *)((char *)free_blk - FOOTER_SIZE); /*prev node footer*/

	uint64_t prev_size = *prev_footer; /*prev node size*/
	
	Header *prev_blk = (Header *)((char *)free_blk - FOOTER_SIZE
						- ALIGN(sizeof(uint64_t))
						- ALIGN(prev_size)); /*prev node header*/

	Header *full_blk = prev_blk;
	
	uint64_t full_size = prev_size + free_blk->flags
					    + ALIGN(sizeof(uint64_t))
						+ FOOTER_SIZE; /*size for full node*/

	blk_remove(prev_blk); /*remove previous node from list*/

	/*change header & footer size*/
	full_blk->flags = full_size;
	Footer *full_footer = (Footer *)((char*)full_blk + HEAP_INC(full_size) - FOOTER_SIZE);
	*full_footer = full_size;

	return full_blk;
}

void*
seg_malloc(size_t size)
{
	assert(size > 0);
	Header *header = get_free_blk(size);

	blk_remove(header);

	mark_alloc(header);

	return (void *)((char *)header + ALIGN(sizeof(uint64_t)));
}

void*
seg_calloc(size_t nmemb, size_t size)
{
	assert(nmemb > 0 && size > 0);
	void *ptr = seg_malloc(nmemb * size);

	assert(ptr != NULL);

	char *temp = (char *)ptr;
	for(size_t i = 0; i < size * nmemb; i++) {
		*temp = (char)0;
		temp++;
	}

	return ptr;
}

void*
seg_realloc(void *ptr, size_t size)
{
	if (ptr == NULL) {
		return seg_malloc(size);
	} else {
		 if (size == 0)
		 	seg_free(ptr);
	}

	Header *header = (Header *)((char *)ptr - ALIGN(sizeof(uint64_t)));
	uint64_t old_size = header->flags & ~3;

	void* new_ptr = seg_malloc(size);
	MEMCPY(new_ptr, ptr, old_size);

	seg_free(ptr);

	return new_ptr;
}

void
seg_free(void *ptr)
{
	assert(ptr != NULL && ptr > heap_start);
	Header *header = (Header *)((char *)ptr - ALIGN(sizeof(uint64_t)));
	assert(header->flags & 1);

	mark_dealloc(header);
	Header *free_blk = header;
	
	if (!(header->flags & 2)) { /*if it's on heap*/
		free_blk = coalesce(header);
		blk_insert(free_blk);
	} else { /*if it's mapped*/
		size_t alloc_size = (size_t)(header->flags & ~3);
		size_t full_size = MAP_INC(alloc_size);
		void *map_ptr = (void *)((char *)ptr - 8);
		int ret = munmap(map_ptr, full_size);
		assert(ret >= 0);
	}
}
