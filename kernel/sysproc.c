#include "types.h"
#include "riscv.h"
#include "param.h"
#include "defs.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

#define MAX_PAGES_SCANNED_NUMBER 128
#define PGACCESS_BUFF_SIZE (MAX_PAGES_SCANNED_NUMBER / 8) + 1
#define PGACCESS_TOTAL_BITS PGACCESS_BUFF_SIZE * 8

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

// TODO: remove
#define LAB_PGTBL
#ifdef LAB_PGTBL
int sys_pgaccess(void)
{
  // getting args
  uint64 userpage;
  uint64 num_of_bits;
  uint64 bitmask_res_addr;
  argaddr(0, &userpage);
  argint(1, &num_of_bits);
  argaddr(0, &bitmask_res_addr);

  int bit = 0;
  int element = 0;
  char buff[PGACCESS_BUFF_SIZE] = {0};
  pte_t *pte;
  pagetable_t pagetable;
  pagetable = myproc()->pagetable;
  for (int i = 0; i < num_of_bits; i++)
  {
    if ((pte = walk(pagetable, i * PGSIZE, 0)) == 0 || (*pte & PTE_V) == 0)
      continue;
    if (PTE_ACCESSED(*pte))
    {
      *pte = *pte & ~(PTE_A);
      // we found a page that was accessed - turn on the corresponding bit
      bit = PGACCESS_TOTAL_BITS - i - 1; // we start from lsb
      element = bit / 8;
      bit = bit % 8;
      buff[element] = buff[element] | (1L << (7 - bit));
    }
  }
  // TODO: copy out the buffer to bitmask_res_addr
  if (copyout(pagetable, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
    return 0;
}
#endif

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
