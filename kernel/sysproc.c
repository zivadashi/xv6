#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#define STARTING_MMAP_ADDR 0x1000000000
#define VMA_ADDR 0x2000000000
#define VMA_COUNT 16

struct vma
{
  uint64 addr;
  int len;
  int prot;
};

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if (n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (killed(myproc()))
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_mmap(void)
{
  int length;
  argint(1, &length);
  int prot;
  argint(2, &prot);
  int flags;
  argint(3, &flags);
  int fd;
  argint(4, &fd);
  int offset;
  argint(5, &offset);
  // getting the VMA
  pte_t *pte = walk(myproc()->pagetable, VMA_ADDR, 1);
  struct vma *pa = (struct vma *)PTE2PA(*pte);
  if (!pa)
  {
    pa = kalloc();
    memset(pa, 0, PGSIZE);
    *pte = *pte | PA2PTE(pa);
  }
  // creating a mapping
  uint64 va = STARTING_MMAP_ADDR;
  for (int i = 0; i < VMA_COUNT; i++)
  {
    if (pa[i].addr == 0)
    {
      pa[i].addr = va;
      pa[i].len = length;
      pa[i].prot = prot;
      break;
    }
    else
    {
      va += pa[i].len + ((PGSIZE - (pa[i].len % PGSIZE)) % PGSIZE);
    }
  }

  return va;
}

uint64
sys_munmap(void)
{
  return -1;
}
