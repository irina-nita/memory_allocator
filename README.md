# Memory Allocator - Segregated List Approach
## 1. About the implementation
```c
  Simple implementation of malloc, realloc, calloc and free using segregated free list, using an
  array of free lists. Each array contains blocks of the same class size (i.e. power of two). The
  "Small" classes have sizes ranging from 16 to 1024 bytes. They are allocated using the sbrk
  system call and can be split and coalesced upon allocating/deallocating to prevent fragmentation.
  
  The "Big" class has sizes larger than 1024 bytes. They are allocated/deallocated using
  mmap/munmap system calls and cannot be split or coalesced.
  Note: Upon coalescing, there can be formed blocks belonging to the "big" class size.
  These blocks can be split/coalesced. The tracking of the type of allocation is kept in
  the 'flags' member of the header struct.
  
  - free block      
  		| --------------------- | ------------------------------------------ | --------|
  		| 8 bytes               | 8 bytes | 8 bytes | aligned to 8 bytes     | 8 bytes |
  		| --------------------- | ----------------- | -----------------------|         |
  		| size         |m/h||a/f| *prev  | *next    | size_of_payload - 16 ||| footer  |
  		| --------------------- | ------------------------------------------ | --------|
  
  - allocated block
  		| --------------------- | ------------------------------------------ | --------|
  		| 8 bytes               | aligned to 8 bytes                         | 8 bytes |
  		| --------------------- | ----------------- | -----------------------|         |
  		| size         |m/h||a/f| payload                       | padding |||| footer  |
  		| --------------------- | ------------------------------------------ | --------|
  
  Note: There is no use in keeping track of next/previous node pointers in allocated blocks,
  so they get overwritten. (The reason why the minimum size for a block is 16 bytes).
  
  Note: The blocks with m flag set as 1 (mmap-ed) have no footer, since there they can't
  be split nor coalesced.
```
```c
/*************************************************************************************************/
  - split
  Upon finding a block in free list that has at least 32 bytes left free after allocating the block
  of needed size, the remaining block is initialized as free and is added to the corresponding
  size class ("bucket").
  
  - before split
    	| -------------- | ---------------------------------------------------------- | --------|
    	| 24 bytes       | aligned to 8 bytes                                         | 8 bytes |
    	| -------------- | ---------------------------------------------------------- |         |
    	| header         | size_of_payload - 16                                     ||| footer  |
    	| -------------- | ---------------------------------------------------------- | --------|

  - after split
    	| --------- | ------------------ | ------- | -------- | ------------------ | ------- |
    	| 8 bytes   | aligned to 8 bytes | 8 bytes | 24 bytes | aligned to 8 bytes | 8 bytes |
    	| --------- | ------------------ | ------- | -------- | ------------------ | ------- |
    	| size      | payload | padding  | footer  | header   | payload | padding  | footer  |
    	| --------- | ------------------ | ------- | -------- | ------------------ | ------- |

     
/*************************************************************************************************/
  - coalescing
  Upon freeing a block, a safe check (between heap_start and heap_end) is made for free blocks
  surrounding (in terms of address). The blocks are coalesced as a new free block, for fragmentation
  avoidance.

  - before coalesce
    	| --------- | ------------------ | ------- | -------- | ------------------ | ------- |
    	| 24 bytes  | aligned to 8 bytes | 8 bytes | 24 bytes | aligned to 8 bytes | 8 bytes |
    	| --------- | ------------------ | ------- | -------- | ------------------ | ------- |
    	| header    | payload | padding  | footer  | header   | payload | padding  | footer  |
    	| --------- | ------------------ | ------- | -------- | ------------------ | ------- |

  - after coalesce

    	| -------------- | ---------------------------------------------------------- | --------|
    	| 24 bytes       | aligned to 8 bytes                                         | 8 bytes |
    	| -------------- | ---------------------------------------------------------- |         |
    	| header         | size_of_payload - 16                                     ||| footer  |
    	| -------------- | ---------------------------------------------------------- | --------|

```
## 2. How to use
The Makefile provided generates an archive file (static library).
```
cd lib/
make
```
To link it, run:
```
gcc -g -c source1.c ... -o test.o
gcc test.o lib/libsegmalloc.a -lm -o test
```
