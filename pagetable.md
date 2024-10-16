# Page Tables
[MIT6.1810 pgtbl Page](https://pdos.csail.mit.edu/6.S081/2022/labs/pgtbl.html)

This lab will introduce you to the page table data structure and how it is used to map virtual addresses to physical addresses.

In this lab, we will often modify several functions. It's better to figure out the mechanism of these functions first, and then modify them.

## Speed up system calls
This task requires us to map a `usyscall` page to the user's address space, and reuse the infomation contained in this page.

In `memlayout.h`, we can see that:
```c
#ifdef LAB_PGTBL
#define USYSCALL (TRAPFRAME - PGSIZE)

struct usyscall {
  int pid;  // Process ID
};
#endif
```

What we need to do is to allocate a page for `usyscall` in each `struct proc` and carefully map it to the user's address space.

In `kernel/proc.h`, we add a field to `struct proc`:
```c
struct proc {
    ...
    struct usyscall *usyscall;  // * syscall speedup page
};
```

In `kernel/proc.c`, we do the following things:
* allocate a page for `usyscall` in `allocproc`
* map the page to the user's address space
* free the page in `freeproc`
* unmap the page in `proc_freepagetable`

```c
static struct proc*
allocproc(void)
{
    ...
    // Allocate a trapframe page.
    if((p->trapframe = (struct trapframe *)kalloc()) == 0){ ... }
    // * Allocate a syscall page
    if((p->usyscall = (struct usyscall *)kalloc()) == 0){
        freeproc(p);
        release(&p->lock);
        return 0;
    }
    p->usyscall->pid = p->pid;
} 

pagetable_t
proc_pagetable(struct proc *p)
{
    ...
    // map the trapframe page just below the trampoline page, for
    // trampoline.S.
    ...

    // * map the usyscall page just below the trapframe page
    if(mappages(pagetable,USYSCALL,PGSIZE,
                (uint64)(p->usyscall), PTE_U | PTE_R) < 0)
    {
        uvmunmap(pagetable, TRAMPOLINE, 1, 0);
        uvmunmap(pagetable, TRAPFRAME, 1, 0);
        uvmfree(pagetable, 0);
        return 0;
    }
    ...
}

void
proc_freepagetable(pagetable_t pagetable, uint64 sz)
{
  uvmunmap(pagetable, TRAMPOLINE, 1, 0);
  // * unmap the syscall page
  uvmunmap(pagetable,USYSCALL,1,0);
  ...
}

static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  // * free the syscall page
  if(p->usyscall)
    kfree((void*)p->usyscall);
  p->usyscall = 0;
  ...
}
```

## Print a Page Table
This task requires us to digest the code in `vm.c`, especially the code walk through pagetables like `freewalk`.

According to the lab page, we should print the first page:
```c
// in exec.c
int
exec(char *path, char **argv)
{
    ...
    // insert here
    if(p->pid==1) vmprint(p->pagetable);
    return argc;
    ...
}
```

Then define a `vmprint` function in `def.h`:
```c
// vm.c
...
int             vmprint(pagetable_t pagetable);
```

Finally, implement the function in `vm.c`:
```c
int 
pgPrint(int level,pagetable_t pagetable)
{
  for (int i = 0;i < 512; i++){
    pte_t pte = pagetable[i];
    if(pte & PTE_V){
      for(int j = 0;j < level; j++){
        printf(" ..");
      }
      uint64 child = PTE2PA(pte);
      printf("%d: pte %p ",i,pte);
      printf("pa %p\n",child);
      if((pte & (PTE_R|PTE_W|PTE_X)) == 0){
      // this PTE points to a lower-level page table.
        pgPrint(level+1,(pagetable_t)child);
      }
    }
  }
  return 0;
}


int vmprint(pagetable_t pagetable)
{
  printf("page table %p\n",pagetable);
  return pgPrint(1,pagetable); 
}
```

## Detect which pages have been accessed
We should implement `sys_pgaccess` in `sysproc.c`.
```c
#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  return 0
}
```
The keypoint is that when we walk through the pagetable, what should we do with the `pte`.
In xv6, the `PTE_A` bit is the 6th bit in the entry, so we define it first in `riscv.h`
```c
...
#define PTE_U (1L << 4) // user can access
// * PTE A is the 6th bit
#define PTE_A (1L << 6) // was accessed
```

Then we begin to implement our `sys_pgaccess` function.

First, we should parse the arguments:
```c

#ifdef LAB_PGTBL
int
sys_pgaccess(void)
{
  // lab pgtbl: your code here.
  struct proc *p = myproc();
  // * Declare the variable to hold the parameters
  uint64 usrePageVa; // the starting virtual address of the first user page to check
  int pageNum; // the number of pages to check
  uint64 userAbits; // a user address to a buffer to store the results into a bitmask
  // * Parse the parameters
  argaddr(0,&usrePageVa);
  argint(1,&pageNum);
  argaddr(2,&userAbits);
  if(pageNum >= 512*512*512){
    printf("Ask to scan too many pages!!!\n");
    return -1;
  }
  // TODO:
  ...
  return 0;
}
```
Then we walk through the pagetable and check the `PTE_A` bit, and copy out the result.
```c
int
sys_pgaccess(void)
{
  ...
  uint64 bitmasks = 0;
  for(int i=0; i < pageNum; i++){
    pte_t *pte = walk(p->pagetable,usrePageVa+i*PGSIZE,0);
    if(*pte & PTE_V){
      if(*pte & PTE_A){ // 1L << 6 according to the spec
        bitmasks |= 1 << i;
        *pte &= ~PTE_A;
      }
    }
    
  }
  // * Copy out the usesrbits finally
  if(copyout(p->pagetable,userAbits,(char *)(&bitmasks),sizeof(bitmasks))){
    return -1;
  }
  return 0;
}
```

## Test
run `make grade` to test your code. Don't forget to add a `time.txt` to get a full score.