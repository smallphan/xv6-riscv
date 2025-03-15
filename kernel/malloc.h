#ifndef _MALLOC_H
#define _MALLOC_H

#include "spinlock.h"
#include "types.h"

#define UNIT_SIZE     32       // * Size of the smallest memory block unit, in bytes.
#define SCALE_NUMBER  20       // * Number of different size scales for memory blocks, ranging from 32B to 16MB.
#define UNIT_NUMBER   524288   // * Total number of memory units available in the heap.
#define HEAP_MASK     0xFFFFFF // * Mask to extract the heap offset within the 16MB heap.

#define UNIT_ROUNDUP(size)    ((size + UNIT_SIZE - 1) / UNIT_SIZE)        // * Rounds up the given size to the nearest multiple of UNIT_SIZE.
#define PAGE_ROUNDUP(size)    ((size + PAGE_SIZE - 1) / PAGE_SIZE)        // * Rounds up the given size to the nearest multiple of PAGE_SIZE.
#define SCALE_TO_SIZE(scale)  (UNIT_SIZE * (1ul << (scale)))              // * Converts a scale index to the corresponding block size in bytes.
#define SCALE_TO_PAGE(scale)  (PAGE_ROUNDUP(SCALE_TO_SIZE(scale)))        // * Converts a scale index to the corresponding number of pages.
#define BLOCK_BIT(scale)      ((1ul << ((scale) + 5ul)) & HEAP_MASK)      // * Calculates the lowest bit of the block index for a given scale.
#define PAGE_INDEX(addr)      (((uint64) addr - STACK_STOP) / PAGE_SIZE)  // * Computes the page index for a given address in the heap.
#define EQUAL_OR_ZERO(a, b)   ((a == b) || (a == 0))                      // * Checks if two values are equal or if the first value is zero.

// * Represents a memory block in the free list.
typedef struct block {
  struct block * next;
} * Block;

// * Represents the free list structure, which 
// * manages memory blocks of different scales.
struct FreeList {
  struct spinlock lock;
  Block scale[SCALE_NUMBER];
  char *procs, *reman;
} freelist;

// * Represents the metadata header for an 
// * allocated memory block.
typedef struct header {
  uint64 scale;
} * Header;

#endif // _MALLOC_H