# Syscall
[MIT6.1810 Syscall Page](https://pdos.csail.mit.edu/6.S081/2022/labs/syscall.html)

This lab will introduce you to writing system calls. 

In summary, we will implement the following syscalls:
* trace
* sysinfo

Before going on, we should learn how to make our syscall compiled to run in xv6.

## Setup a syscall
In `user/user.h`, declare your syscall prototype:
```c
// syscalls
...
// your syscall prototype
int trace(int);
int sysinfo(struct sysinfo *); // struct info should be declared at the beginning
...
```

In `kernel/syscall.h`, add your syscall number:
```c
// syscall numbers
...
// your syscall number
#define SYS_trace 22
#define SYS_sysinfo 23
...
```

In `kernel/syscall.c`, add your syscall to the syscall table:
```c
// Prototypes for the functions that handle system calls.
...
extern uint64 sys_trace(void);
extern uint64 sys_sysinfo(void);

// An array mapping syscall numbers from syscall.h
// to the function that handles the system call.
static uint64 (*syscalls[])(void) = {
    ...
    [SYS_trace]   sys_trace,
    [SYS_sysinfo]  sys_sysinfo,
}
```

In `kernel/sysproc.c`, implement your syscall:
```c
...
uint64
sys_trace(void)
{
    ...
}

uint64
sys_sysinfo(void)
{
    ...
}
```

Finally, add an entry to `user/usys.pl`
```perl
...
entry("sysinfo");
entry("trace");
```
## trace
add `trace` as described in the previous part.

First, add a `mask` in `struct proc`:
```c
struct proc {
    ...
    int mask; // mask for trace
    ...
}
```

Then, in `sys_trace`, set the mask:
```c
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  myproc()->mask = mask;
  return 0;
}
```

In `fork()`, copy the mask from parent to child:
```c
// Create a new process, copying the parent.
// Sets up child kernel stack to return as if from fork() system call.
int
fork(void)
{
    ...
    // copy saved user registers.
    *(np->trapframe) = *(p->trapframe);

    // Copy mask from parent to child
    np->mask = p->mask;
    ...
}
```

Finally, in `syscall()`, print the info if the mask is set:
```c
void
syscall(void)
{
    ...
  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    // Use num to lookup the system call function for num, call it,
    // and store its return value in p->trapframe->a0
    p->trapframe->a0 = syscalls[num]();
    if ((p->mask >> num )& 1) {
      printf("%d: syscall %s -> %d\n",p->pid,syscallNames[num],p->trapframe->a0);
    }
  } else { ... }
}
```

## sysinfo
add `sysinfo` as described in the previous part.

First, take a look at `struct sysinfo` in `sysinfo.h`
```c
struct sysinfo {
  uint64 freemem;   // amount of free memory (bytes)
  uint64 nproc;     // number of process
};
```
In the lab page, the fields are described as:
> the freemem field should be set to the number of bytes of free memory, and the nproc field should be set to the number of processes whose state is not UNUSED

To accomplish this, we need to implement two functions:
* `kfree_space()`: get the number of free pages
* `used_procs()`: get the number of used processes

First, we define these 2 functions in `def.h`
```c
// kalloc.c
...
int kfree_space(void);
...
// proc.c
...
int used_procs(void);
```

Then we implement them in `kalloc.c` and `proc.c`:
```c
// kalloc.c
int kfree_space() {
  // Get the size of freelist
  acquire(&kmem.lock);
  int size = 0;
  struct run *r = kmem.freelist;
  while (r) {
    size++;
    r = r->next;
  }
  release(&kmem.lock);
  return size * PGSIZE;
}

// proc.c
int used_procs(void) {
  // Find the total number of unused processes
  int n = 0;
  for (int i = 0; i < NPROC; i++) {
    acquire(&proc[i].lock);
    if (proc[i].state != UNUSED) {
      n++;
    }
    release(&proc[i].lock);
  }
  return n;
}
```
Note that locks are needed.

Finally, we implement `sys_sysinfo`:
```c
uint64
sys_sysinfo(void)
{
  // Get infos
  struct sysinfo info;
  info.freemem = kfree_space(); // numOfElem in freelist * PGSIZE
  info.nproc = unused_procs(); // Traverse the proc list
  // Get User pointer to sysinfo
  struct proc *p = myproc();
  uint64 usrInfo;
  argaddr(0,&usrInfo); // argument 0,not 1
  // Copy info to usrinfo
  if (copyout(p->pagetable,usrInfo,(char* )&info,sizeof(info))){
    return -1;
  }
  return 0;
}
```

The usage of `copyout` can be found at `sys_fstat()`

## Test
run `make grade` to test your code. Don't forget to add a `time.txt` to get a full score.