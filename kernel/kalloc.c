// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct kmem {
  struct spinlock lock;
  struct run *freelist;
};

struct kmem kmem_arr[NCPU];

static struct spinlock remaining_lock;
static int remaining_mem_total = 0;

// getting the amount of free pages this cpu has
int get_free_mem_amount(struct run *freelist){
  int count = 0;
  struct run* curr = freelist;
  while(curr){
    curr = curr->next;
    count++;
  }
  return count;
}

// iterating over the cpus and finding the one with the most free memory
int find_cpu_with_most_memory(){
  int res = 0;
  int highest = 0;
  int curr_count = 0;
  for (int i = 0; i < NCPU; i++){
    curr_count = get_free_mem_amount(kmem_arr[i].freelist);
    if (curr_count > highest){
      highest = curr_count;
      res = i;
    }
  }
  return res;
}

// stealing half of the available memory from src
void steal_memory(struct kmem* dst, struct kmem* src){
  int free_count = get_free_mem_amount(src->freelist);
  if (free_count == 0){
    // nothing to steal
    return;
  }
  if (free_count == 1){
    // stealing all the memory
    dst->freelist = src->freelist;
    src->freelist = 0;
    return;
  }
  int steal_amount = free_count / 2;
  // getting to the stealing index
  struct run* curr = src->freelist;
  for (int i = 0; i < steal_amount - 1; i++){
    curr = curr->next;
  }
  // stealing the rest of the list
  dst->freelist = curr->next;
  curr->next = 0;
}

// getting more memory for the cpu with the given id
void get_memory(int id){
  // starting the search from id + 1
  for (int i = (id + 1) % NCPU; i != id; i = (i + 1) % NCPU){
    if (!try_lock(&kmem_arr[i].lock)){
      // didn't manage to lock
      continue;
    }
    if (!kmem_arr[i].freelist){
      // doesn't have free memory
      release(&kmem_arr[i].lock);
      continue;
    }
    // stealing
    steal_memory(&kmem_arr[id], &kmem_arr[i]);
    release(&kmem_arr[i].lock);
    break;
  }
}

void
kinit()
{
  for (int i = 0; i < NCPU; i++){
    initlock(&kmem_arr[i].lock, "kmem");
  }
  freerange(end, (void*)PHYSTOP);
  initlock(&remaining_lock, "rem");
  push_off();
  int id = cpuid();
  pop_off();
  remaining_mem_total = get_free_mem_amount(kmem_arr[id].freelist);
  printf("starting memory: %d\n", remaining_mem_total);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by pa,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  // getting current cpu id
  push_off();
  int id = cpuid();
  pop_off();

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem_arr[id].lock);
  r->next = kmem_arr[id].freelist;
  kmem_arr[id].freelist = r;
  release(&kmem_arr[id].lock);
  acquire(&remaining_lock);
  remaining_mem_total++;
  release(&remaining_lock);

}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  // getting current cpu id
  push_off();
  int id = cpuid();
  pop_off();

  int allocated = 0;

  while (!allocated){
    int has_mem = 0;
    acquire(&kmem_arr[id].lock);
    r = kmem_arr[id].freelist;
    if(r){
      //printf("cpu %d allocated\n", id);
      kmem_arr[id].freelist = r->next;
      allocated = 1;
    }
    else{
      // we need to steal memory
      get_memory(id);
      if (!kmem_arr[id].freelist){
        printf("cpu %d stole and failed\n", id);
        has_mem = find_cpu_with_most_memory();
        printf("cpu %d has memory\n", has_mem);
        printf("remaining: %d\n", remaining_mem_total);
      }
      // else{
      //   printf("cpu %d stole and succeeded\n", id);
      // }
    }
    release(&kmem_arr[id].lock);
    //   yield();
    // }
  }
  acquire(&remaining_lock);
  remaining_mem_total--;
  release(&remaining_lock);
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
