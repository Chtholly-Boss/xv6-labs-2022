# Copy on Write
[MIT6.1810 Copy On Write Page](https://pdos.csail.mit.edu/6.S081/2022/labs/cow.html)

This lab focuses on implementing copy-on-write fork for xv6.

Damn it complex...

First, we need a PTE dirty bit to indicate whether a page is dirty.
Refer to the RISC-V manual, you can figure out where to put this bit:
In `riscv.h`, add:
```c
// a record for each PTE that indicates whether it is a cow mapping
// in risc-v PTE, reserved software bits in the PTE are [9:8]
// add a mark to riscv.h
#define PTE_COW (1L << 8) // * whether is a cow mapping
```

Then, we need to count the reference to each page, so we should add a list in `kalloc.c`:
```c
// * Maintain a reference count for each page.
int refCnts[(PHYSTOP -KERNBASE) / PGSIZE] = {0};
```

And modify `kalloc` to initialize the reference count:
```c
void *
kalloc(void)
{
  ...
  if(r){
    memset((char*)r, 5, PGSIZE); // fill with junk
    refCnts[REF_INDEX((uint64)r)] = 1; // * initialize the ref cnt to 1
  }
  return (void*)r;
}
```

Note that we define a macro to index the `refCnts` array:
```c
#define REF_INDEX(pa) (PGROUNDDOWN(pa)- KERNBASE)/PGSIZE
```

We should carefully handle the reference count when we free a page:
```c
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");
  
  uint64 refOffset = REF_INDEX((uint64)pa);

  if(refCnts[refOffset] <= 1){
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);
    refCnts[refOffset] = 0; // * set the ref cnt to 0

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  } else {
    refCnts[refOffset] -= 1;
  }
}
```

Now we can implement the `uvmcopy` function in `vm.c`:
* map parent pages to child, but not allocate new memory
* clear `PTE_W` if exists
* set `PTE_COW` to indicate that it is a cow page
* increment the reference count

```c
extern int refCnts[]; // kalloc.c maintains a reference count for each page
...
int
uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  for(i = 0; i < sz; i += PGSIZE){
    if((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if(flags & PTE_W) {
      // * set cow 
      flags &= ~ PTE_W;
      flags |= PTE_COW;
      *pte &= ~PTE_W;
      *pte |= PTE_COW;
    }
    // Simply map, not allocate new memory
    if(mappages(new, i, PGSIZE, pa, flags) != 0){ 
      goto err;
    }
    // * increment ref count 
    refCnts[REF_INDEX(pa)] += 1;
  } 
  return 0;
err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1; 
}
```

Then we write the the page, we will encounter a page fault.
When this happens, we should:
* detect page-fault exception
  * in risc-v, `scause[XLEN-1]=0`
  * `scause[XLEN-2:0]=12`: instruction page fault
  * `scause[XLEN-2:0]=13`: load page fault
  * `scause[XLEN-2:0]=15`: store page fault
  * for cow, it should be a **store page fault**, so check `scause==15`
* handle page-fault exception
  * check whether it is a cow page-fault by checking PTE_COW
  * if yes
    * allocate a new page:if failed, kill the process
    * copy from old page to new page
    * set new page the necessary `PTE_W` bits
    * unmap old page and install new page
  * if not
    * just kill the process

```c
void
usertrap(void)
{
  int which_dev = 0;

  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // send interrupts and exceptions to kerneltrap(),
  // since we're now in the kernel.
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();
  
  // save user program counter.
  p->trapframe->epc = r_sepc();
  
  if(r_scause() == 15){
    // * a store page-fault
    if(killed(p)){
      exit(1);
    }

    pte_t *pte;
    uint64 addr = PGROUNDDOWN(r_stval());
    if(addr >= MAXVA){
      printf("usertrap: Outbound the MAXVA\n");
      setkilled(p);
    } else if((pte = walk(p->pagetable,addr,0)) == 0){
      printf("usertrap: pte should exist\n");
      setkilled(p);
    } else if((*pte & PTE_V) == 0){
      printf("usertrap: page not present\n");
      setkilled(p);
    } else if((*pte & PTE_COW) == 0){
      printf("usertrap: Not a cow pagefault\n");
      setkilled(p);
    } else {
      char* mem;
      uint flags;
      uint64 pa;
      pa = PTE2PA(*pte);
      if((mem = kalloc()) == 0){
        printf("usertrap: No more memory\n");
        setkilled(p);
      } else {
        memmove(mem,(char*)pa,PGSIZE);

        flags = PTE_FLAGS(*pte);
        flags |= PTE_W; // * set the PTE_W
        flags &= ~PTE_COW; // * unset the PTE_COW
        // * remap to a new page
        uvmunmap(p->pagetable,addr,1,1);
        if(mappages(p->pagetable,addr,PGSIZE,(uint64)mem,flags) != 0){
          panic("usertrap: map failed");
          kfree(mem);
          setkilled(p);
        }
      }
    }
  } else if(r_scause() == 13){
    // a load page fault
    printf("usertrap: load page fault\n");
    setkilled(p);
  } else if(r_scause() == 8){
    // system call

    if(killed(p))
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sepc, scause, and sstatus,
    // so enable only now that we're done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);
  }
  ...
}
```

Finally, we add `copy on write` to `copyout`:
```c
int
copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  ...
  while(len > 0){
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if(pa0 == 0)
      return -1;
    if((pte = walk(pagetable,va0,0)) == 0){
      panic("copyout: pte should exist");
    }
    flags = PTE_FLAGS(*pte);
    if((*pte & PTE_V) == 0){
      panic("copyout: page not present");
    }
    if(flags & PTE_COW){
      char* mem;
      if((mem = kalloc()) == 0){
        // ! no more free space,kill p
        panic("copyout: No more memory!");
      } else {
        memmove(mem,(char*)pa0,PGSIZE);
        flags |= PTE_W; // * set the PTE_W
        flags &= ~PTE_COW; // * unset the PTE_COW
        // * remap to a new page
        uvmunmap(pagetable,PGROUNDDOWN(va0),1,1);
        if(mappages(pagetable,PGROUNDDOWN(va0),PGSIZE,(uint64)mem,flags) != 0){
          panic("copyout: map failed");
          kfree(mem);
        }
        pa0 = (uint64)mem;
      }
    }
    n = PGSIZE - (dstva - va0);
    ...
  }
  return 0;
}
```

## Test
run `make grade` to test your code. Don't forget to add a `time.txt` to get a full score.
