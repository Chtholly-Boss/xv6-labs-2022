# Mmap
This lab is the most tricky one in the course. 

We will implement syscall `mmap` and `munmap` in this lab.
> They can be used to share memory among processes, 
> to map files into process address spaces, 
> and as part of user-level page fault schemes such as the garbage-collection algorithms discussed in lecture. 

In this lab, we'd better follow a *TDD* way, i.e. write code to pass grade tests.

First, we should make things alive:
* adding _mmaptest to UPROGS
* register mmap and munmap system calls

Then, we should make up a place for the `mmap` pages to reside in.
In `proc.h`, we add Virtual Memory Area (VMA) to the `proc` struct.
```c
// * virtual memory area
#define VMA_NUM 16
struct vma {
  uint64 addr; // where the file is mapped
  int permission; // the area should be readable/writeble
  int flag; // map shared or private
  int length;
  struct file *file; // the file corresponding to the area
};

// Per-process state
struct proc {
  ... 
  char name[16];               // Process name (debugging)
  // * proc's Virtual Memory Area
  struct vma vmas[VMA_NUM];
};
```

In `proc.c`, we define a handy function to traverse this VMA array.
Don't forget to add the prototype in `def.h`

```c
/*
 * Find a vma that contains the virtual address va
 */
struct vma* find_vma(uint64 va)
{
  struct proc *p = myproc();
  for(int i = 0; i < VMA_NUM; i++){
    if(p->vmas[i].file){
      if(va >= p->vmas[i].addr && va < (p->vmas[i].addr + p->vmas[i].length)){
        return &p->vmas[i];
      }
    }
  }
  return 0;
}
```
Note that to use `struct file`, we need to add several headers.
```c
// * new headers
#include "fcntl.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
```
You may add these in several files, in the following we will not mention this point again.

We will do **Lazy Allocation** for `mmap`, so add a `PTE_M` in `riscv.h` like we do in [lab: Copy on Write](./cow.md)
```c
...
#define PTE_U (1L << 4) // user can access
#define PTE_M (1L << 8) // * mmap flag
```

Then we implement the `sys_mmap`:
```c
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
```
Comments are added in the code. It's easy to understand how we do this.

Then in `trap.c`, we will do things much like `copy on write`:
```c
void
usertrap(void)
{
    ...
  if(r_scause() == 13) {
    if(killed(p)){
      exit(1);
    }

    pte_t *pte;
    uint64 addr = PGROUNDDOWN(r_stval());
    if (
        (addr >= MAXVA) || 
        (pte = walk(p->pagetable,addr,0)) == 0 ||
        (*pte & PTE_V) == 0 || 
        (*pte & PTE_M) == 0
      ){
      setkilled(p);
    } else {
      // ! load page fault caused by mmap
      struct vma* area = find_vma(addr);
      if(area == 0){
        printf("usertrap: vma should exist\n");
        setkilled(p);
      } else {
        char* mem;
        if((mem = kalloc()) == 0){
          printf("usertrap: No more memory\n");
          setkilled(p);
        } else {
          memset(mem, 0, PGSIZE);
          struct file* f = area->file;
          if (f) {
            int r = 0;

            ilock(f->ip);
            if((r = readi(f->ip, 0, (uint64)mem, addr-area->addr, PGSIZE)) < 0){
              panic("usertrap: readi failed");
            }
            // printf("r: %d\n", r);
            iunlock(f->ip);

            uint flags = PTE_FLAGS(*pte);
            if (area->permission & PROT_WRITE)
              flags |= PTE_W;
            if (area->permission & PROT_READ)
              flags |= PTE_R;
            if (area->permission & PROT_EXEC)
              flags |= PTE_X;

            // map the newly allocated page to addr
            uvmunmap(p->pagetable,addr,1,0);
            if(mappages(p->pagetable,addr,PGSIZE,(uint64)mem,flags) != 0){
              panic("usertrap: map failed");
              kfree(mem);
              setkilled(p);
            }
            
          }
        }
      }
    }
  } else if(r_scause() == 8){
    ...
  }
  ...
}
```

Damn it long... It can be refact to be compact, but I'm tired to do that.

Then we implement `munmap`:
```c
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
```

Finally, obeying the hints, we modify `exit`:
```c
void
exit(int status)
{
  struct proc *p = myproc();

  // Close all open files.
  ...
  // - unmap all vmas
  struct vma *v;
  for(int i = 0; i < VMA_NUM; i++){
    if(p->vmas[i].file){
      v = &p->vmas[i];
      pte_t *pte;
      for(int j = 0; j < v->length; j += PGSIZE){
        pte = walk(p->pagetable,v->addr+j,0);
        if((*pte) != 0){
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
              r = writei(v->file->ip, 1, v->addr+j+off, j+off, n1);
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
          uvmunmap(p->pagetable,v->addr+j,1,(*pte & (PTE_R | PTE_W)));
        }
      }
      fileclose(v->file);
      v->file = 0;
    }
  }
  ...
}
```

And `fork`:
```c
int
fork(void)
{
  ...
  // * copy vmas 
  struct vma *v;
  for(int i = 0; i < VMA_NUM; i++){
    if(p->vmas[i].file){
      v = &p->vmas[i];
      np->vmas[i] = *v;
      if((mappages(np->pagetable,v->addr,v->length,0,PTE_U | PTE_M)) != 0){
        printf("fork: mappage failed\n");
      }
      filedup(np->vmas[i].file);
    }
  }
  ...
}
```
