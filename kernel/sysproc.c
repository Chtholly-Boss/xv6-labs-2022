#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
// ! add new header
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
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
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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
  struct proc *p = myproc();
  // * parse the parameters 
  int length,prot,flags,fd;
  argint(1,&length);
  argint(2,&prot);
  argint(3,&flags);
  argint(4,&fd);

  struct file *f = p->ofile[fd];
  filedup(f);

  if((flags & MAP_SHARED) && !(f->writable) && (prot & PROT_WRITE)){
    // ! permission error, file couldn't be written
    return -1;
  }
  pte_t *pte;
  uint64 a = 0;

  int len = 0;

  int iteration = 0;
  while(len < length){
    if(iteration++ > 10)
      panic("mmap: no continous unused region");
    // * find an unused region in the address space to map the file
    while((pte = walk(p->pagetable,a,0)) != 0){
      a += PGSIZE;
    }
    // * already find the first unused address 
    // ! if length > PGSIZE, you may need to find a continous region
    for(; len < length; len += PGSIZE){
      if((pte = walk(p->pagetable,a + len,0)) != 0){
        // ! not a continous unused region
        len = 0;
        break;
      }
    }
  }
  // * add a vma to proc's table
  struct vma *v = 0;
  for(int j = 0; j < VMA_NUM; j++){
    if(!(p->vmas[j].file)){
      // * not used
      v = &p->vmas[j];
      break;
    }
  }
  if(v){
    v->length = length;
    v->addr = a;
    v->file = f;
    v->permission = prot;
    v->flag = flags;
  } else {
    printf("mmap: no unused vma\n");
    return (uint64) -1;
  }
  // * map the needed pages
  if((mappages(p->pagetable,v->addr,length,0,PTE_U | PTE_M)) != 0){
    printf("mmap: map failed\n");
    return (uint64)-1;
  }
  return v->addr;
}

uint64
sys_munmap(void)
{
    struct proc *p = myproc();
  uint64 addr;
  int len;
  argaddr(0,&addr);
  argint(1,&len);

  addr = PGROUNDDOWN(addr);
  struct vma *v;
  if((v = find_vma(addr))){
    // - unmap the pages
    pte_t *pte;
    for(int i = 0; i < len; i += PGSIZE){
      if((pte = walk(p->pagetable,addr+i,0)) == 0){
        printf("munmap: page not mapped\n");
        return -1;
      }
      if((v->flag & MAP_SHARED) && (*pte & PTE_W)){
        int r = 0;
        // ! map shared, write to the file
        int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
        int off = 0;
        while(off < PGSIZE){
          int n1 = PGSIZE - off;
          if(n1 > max)
            n1 = max;
          begin_op();
          ilock(v->file->ip);
          r = writei(v->file->ip, 1, addr+i+off, (addr+i - v->addr+off), n1);
          iunlock(v->file->ip);
          end_op();
          if(r != n1){
            // error from writei
            printf("munmap: error on writing to file\n");
            break;
          }
          off += r;
        }
      }
      uvmunmap(p->pagetable,addr+i,1,(*pte & (PTE_R | PTE_W)));
    }
    // - page all unmapped, close the file
    int close = 1;
    for(int i = 0; i < v->length; i += PGSIZE){
      if((pte = walk(p->pagetable,v->addr+i,0)) != 0)
        close = 0;
    }
    if(close){
      fileclose(v->file);
      v->file = 0;
    }
  }else{
    printf("munmap: not a mmap addr\n");
    return -1;
  }
  return 0;
}