#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

#define MAP_SIZE NPROC // * Defines the maximum size of the shared memory map, equal to the number of processes (NPROC).

// * Represents the shared memory mapping table, including a spinlock for synchronization.
// * Each entry contains:
// * - id: Unique identifier for the shared memory.
// * - pa: Physical address of the shared memory.
// * - ct: Reference count of processes using the shared memory.
struct {
  struct spinlock lock;
  struct {
    uint64 id, pa, ct;
  } map[MAP_SIZE];
} map;

// * Searches for a shared memory entry in the map by its unique ID.
// * Returns the index of the entry if found, or -1 if not found.
int 
map_find(
  uint64 id
) {
  for (int i = 0; i < MAP_SIZE; i++) {
    if (map.map[i].id == id) {
      return i;    
    }
  }
  return -1;
}

// * Generates a unique ID based on the given hash value using a hash function and a static seed.
// * Ensures uniqueness across calls.
uint64 
unique_id(
  uint64 hash
) {
  const uint64 prime = 1099511628211ULL; 
  static uint64 seed = 0;

  hash += seed;
  hash ^= (hash >> 30);
  hash *= prime;
  hash ^= (hash >> 27);
  hash *= prime;
  hash ^= (hash >> 31);

  return (seed = hash);
}

// * Joins a process to a shared memory segment with the given ID.
// * Maps the shared memory to the process's page table and increments the reference count.
// * Returns the virtual address of the shared memory.
void*
shmjoin(
  uint64 id
) {
  struct proc *p = myproc();
  
  int idx = map_find(id);
  uint64 ans = 0;

  if (p->shm == map.map[idx].id) {
    // ! Share memory has joined.
    ans = SHARE_MEMORY;
  } else if(p->shm != 0) {
    panic("proc has already allocated a shared memory page.");
  } else {
    p->shm = map.map[idx].id;
    map.map[idx].ct++;
    mappages(p->pagetable, SHARE_MEMORY, PAGE_SIZE, map.map[idx].pa, PTE_U | PTE_R | PTE_W);
    ans = SHARE_MEMORY;
    // # printf("[KERNEL] idx %d, pid %d, mapping %p to %p\n", idx, p->pid, SHARE_MEMORY, map.map[idx].pa);
  }

  return (void*) ans; 
}

// * Allocates a new shared memory page for the calling process.
// * Finds an empty slot in the map, generates a unique ID, and maps the memory to the process.
// * Returns the unique ID of the allocated shared memory.
uint64 
shmget(
  void
) {
  struct proc *p = myproc();

  if (p->shm) {
    panic("proc has already allocated a shared memory page.");
    return 0;
  }

  int idx; uint64 ans;
  if (((idx = map_find(0)) != -1)) {
      map.map[idx].pa = (uint64) kalloc();
      ans = map.map[idx].id = unique_id(map.map[idx].pa);
      shmjoin(map.map[idx].id);
  } else panic("unable to find map by index.");

  return ans;
}

// * Frees the shared memory allocated by the calling process.
// * Unmaps the shared memory from the process's page table and decrements the reference count.
// * If the reference count reaches zero, the memory is deallocated.
void 
shmfree(
  void
) {
  struct proc *p = myproc();

  int idx;
  if ((p->shm != 0) && (idx = map_find(p->shm)) != -1) {
    *walk(p->pagetable, SHARE_MEMORY, PAGE_SIZE) = 0;
    p->shm = 0;
    if (--map.map[idx].ct == 0) {
      kfree((void*) map.map[idx].pa);
      // # printf("Freeing... id: %p, pa: %p\n", map.map[idx].id, map.map[idx].pa);
      map.map[idx].id = 0;
      map.map[idx].pa = 0;
    }
  } else if (p->shm == 0) {
    // ! No alloced share memory.
  } else panic("unable to find map by index.");
}

// * System call to join a shared memory segment by its ID.
// * Acquires the map lock, calls `shmjoin`, and releases the lock.
// * Returns the virtual address of the shared memory.
void*
sys_shmjoin(
  void 
) {
  uint64 id;
  argaddr(0, &id);

  acquire(&map.lock);
  void *ans = shmjoin(id);
  release(&map.lock);

  return ans;
}

// * System call to allocate a new shared memory page.
// * Acquires the map lock, calls `shmget`, and releases the lock.
// * Returns the unique ID of the allocated shared memory.
uint64
sys_shmget(
  void
) {
  acquire(&map.lock);
  uint64 ans = shmget();
  release(&map.lock);

  return ans;
}

// * System call to free the shared memory allocated by the calling process.
// * Acquires the map lock, calls `shmfree`, and releases the lock.
void 
sys_shmfree(
  void
) {
  acquire(&map.lock);
  shmfree();
  release(&map.lock);
}