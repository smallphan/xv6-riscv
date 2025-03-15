#include "types.h"
#include "malloc.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "proc.h"
#include "defs.h"

#define DEBUG_MODE

// * Removes and returns the first block from the 
// * free list for the given scale.
uint64
list_pop(
  int scale
) {
  Block ans = freelist.scale[scale];
  freelist.scale[scale] = ans->next;
  return (uint64) ans;
}

// * Adds a block back to the free list for the 
// * given scale.
void
list_push(
  uint64 block,
  int    scale
) {
  Block temp = (Block) block;
  temp->next = freelist.scale[scale];
  freelist.scale[scale] = temp;
}

// * Searches for the buddy block of a given block 
// * in the free list and removes it if found.
uint64
list_find_buddy(
  uint64 block,
  int    scale
) {
  Block temp = freelist.scale[scale];
  if (temp) {
    if ((uint64) temp == block) return list_pop(scale);
    for (; temp->next; temp = temp->next) {
      if ((uint64) temp->next == block) {
        Block ans = temp->next;
        temp->next = ans->next;
        return (uint64) ans;
      }
    }
  }
  return 0;
}

// * Searches for a block in the free list that 
// * matches the given index and removes it if 
// * found.
uint64
list_find_index(
  int index,
  int scale
) {
  Block temp = freelist.scale[scale];
  if (temp) {
    if (EQUAL_OR_ZERO(freelist.procs[PAGE_INDEX((uint64) temp)], index)) return list_pop(scale);
    for (; temp->next; temp = temp->next) {
      if (EQUAL_OR_ZERO(freelist.procs[PAGE_INDEX((uint64) temp->next)], index)) {
        Block ans = temp->next;
        temp->next = ans->next;
        return (uint64) ans;
      }
    }
  }
  return 0;
}

// * Defines the debug information levels for 
// * displaying memory-related information.
enum InfoLevel { 
  INFO_MESAG = 0 << 0,
  INFO_SCALE = 1 << 0,
  INFO_TABLE = 1 << 1,
  INFO_REMAN = 1 << 2
};

// * Displays debug information about the memory 
// * allocator based on the specified mode.
void
list_info(
  int   mode,
  char* message
) {
  acquire(&freelist.lock);

  printf("\n-- Debug Output ↓ --\n\n");
  printf(">>> %s\n", message);

  // Blocks of different scales in freelist
  if (mode & INFO_SCALE) {
    printf("\n(Blocks of different scales in freelist)\n\n");
    for (int i = 0; i < SCALE_NUMBER; i++) {
      printf("Scale %s%d: ", (i < 10 ? " " : ""), i);
      for (Block temp = freelist.scale[i]; temp; temp = temp->next) {
        printf("%p ", temp);
      }
      printf("\n");
    }
  }

  // Occupier of each Page in HEAP
  if (mode & INFO_TABLE) {
    printf("\n(Occupier of each Page in HEAP)\n");
    for (int i = 0; i < PAGE_SIZE; i++) {
      if (i % 64 == 0) printf("\n");
      printf("%d ", (int) freelist.procs[i]);
    }
    printf("\n");
  }

  // Remaining occupied blocks of each Page in HEAP 
  if (mode & INFO_REMAN) {
    printf("\n(Remaining occupied blocks of each Page in HEAP)\n");
    for (int i = 0; i < PAGE_SIZE; i++) {
      if (i % 64 == 0) printf("\n");
      printf("%d ", (int) freelist.reman[i]);
    }
    printf("\n");
  }

  printf("\n-- Debug Output ↑ --\n\n");
  release(&freelist.lock);
}

// * Initializes the memory allocator, including 
// * the free list and metadata tables.
void
init_malloc(
  void
) {
  initlock(&freelist.lock, "malloc lock");
  
  freelist.scale[SCALE_NUMBER - 1] = (Block) STACK_STOP;
  memset(freelist.scale[SCALE_NUMBER - 1], 0, HEAP_SIZE);
  
  freelist.procs = (char*) kalloc();
  memset(freelist.procs, 0, PAGE_SIZE);

  freelist.reman = (char*) kalloc();
  memset(freelist.reman, 0, PAGE_SIZE);
}

// * Allocates a memory block of the specified size 
// * and returns a pointer to it.
void*
malloc(
  uint64  size,
  uint64* ret_scale
) {
  int unit = UNIT_ROUNDUP(size);
  struct proc* p = myproc();

  if (unit > UNIT_NUMBER) {
    panic("malloc: malloc size beyond heap size");
    return 0;
  }

  int scale = 0, minscale = -1;
  acquire(&freelist.lock);
  Block block;

  for (; scale < SCALE_NUMBER; scale++) {
    if ((1 << scale) >= unit) {
      if (minscale < 0) minscale = scale;
      if ((block = (Block) list_find_index(proc_index(p), scale))) goto found;
    }
  }

  release(&freelist.lock);
  panic("malloc: could not find sufficient space");
  return 0;

found:

  for (int i = scale; i > minscale; i--) {
    list_push((uint64) block + SCALE_TO_SIZE(i - 1), i - 1);
  }

#ifdef DEBUG_MODE
  printf("\n-- Debug Output ↓ --\n\n");
  printf("minscale: %d, scale %d\n", minscale, scale);
  printf("Malloc range(%p, %p)\n", block, (uint64) block + SCALE_TO_SIZE(minscale) - 1);
  printf("\n-- Debug Output ↑ --\n\n");
#endif // DEBUG_MODE

  *ret_scale = minscale;
  memset(freelist.procs + PAGE_INDEX(block), proc_index(p), SCALE_TO_PAGE(minscale)); 

  release(&freelist.lock);
  return block;
}

// * Allocates a memory block with additional 
// * metadata and maps it to the process's page 
// * table.
void*
malloc_wrapper(
  uint64 size
) {
  uint64 scale;
  Header head = (Header) malloc(size + sizeof(Header), &scale);
  struct proc *p = myproc();
  
  if (!head) return 0;
  head->scale = scale;

  // Mapping malloced space to proc pagetable
  if (freelist.reman[PAGE_INDEX(head)]++ == 0) 
    mappages(p->pagetable, (uint64) head, size + sizeof(Header), (uint64) head, PTE_U | PTE_R | PTE_W);
  
  return (void*) (head + 1);
}

// * Attempts to merge a block with its buddy block 
// * and returns the merged block address if 
// * successful.
uint64
merge_buddy(
  uint64 addr,
  int    scale
) {
  uint64 heapoffset = addr & HEAP_MASK;
  uint64 heapnumber = addr - heapoffset;
  uint64 buddy = (heapoffset ^ BLOCK_BIT(scale)) + heapnumber;
  if (list_find_buddy(buddy, scale)) {
    return (buddy & ~BLOCK_BIT(scale));
  } else return 0;
}

// * Frees a memory block and attempts to merge it 
// * with its buddy blocks.
void
free(
  void* ptr,
  int   sed_scale
) {
  uint64 addr = (uint64) ptr, ans;
  int scale = sed_scale;
  acquire(&freelist.lock);
  for (; scale < SCALE_NUMBER; scale++) {
    if ((ans = merge_buddy(addr, scale))) {
      addr = ans;
    } else {
      list_push(addr, scale);
      goto success;
    }
  }

success:
  memset(freelist.procs + PAGE_INDEX((uint64) ptr), 0, SCALE_TO_PAGE(sed_scale));
  release(&freelist.lock);
}

// * Frees a memory block, unmaps it from the 
// * process's page table, and merges buddy blocks 
// * if possible.
void
free_wrapper(
  void* ptr
) {
  Header head = (Header) ptr - 1;
  struct proc *p = myproc();

  // Unmapping pages from proc pagetable
  acquire(&freelist.lock);
  if (--freelist.reman[PAGE_INDEX((uint64) head)] == 0) {
    for (uint64 va = (uint64) head; va <= (uint64) head + SCALE_TO_SIZE(head->scale) - 1; va += PAGE_SIZE) 
      *walk(p->pagetable, va, 0) = 0;
  }
  release(&freelist.lock);

  free((void*) head, head->scale);
}

// * System call for allocating memory. Initializes 
// * the allocated memory to zero.
void* 
sys_malloc(
  void
) {
  uint64 size;
  argaddr(0, &size);
  
  void* ans = malloc_wrapper(size);
  memset(ans, 0, size);

#ifdef DEBUG_MODE
  list_info(INFO_SCALE, "System Malloc:");
#endif // DEBUG_MODE

  return ans;
}

// System call for freeing memory.
void
sys_free(
  void
) {
  uint64 ptr;
  argaddr(0, &ptr);
  
  free_wrapper((void*) ptr);
  
#ifdef DEBUG_MODE
  list_info(INFO_SCALE, "System Free:");
#endif // DEBUG_MODE
}