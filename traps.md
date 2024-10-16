# Traps
[MIT6.1810 Traps Page](https://pdos.csail.mit.edu/6.S081/2022/labs/traps.html)

This lab introduces the mechanism of `usertraps`, specifically, the `timer interrupt`.

## RISC-V Assembly
Answers are stored in the `answers-traps.txt` file in the branch `traps`. 

You can refer to the [Lecture Note](https://pdos.csail.mit.edu/6.1810/2022/lec/l-riscv.txt) or the xv6 Book to get the answers, here we omit the re-description of the relevant details.

## Backtrace
We will implement a kernel function called `backtrace` to print the stack trace of the current process.

First, add a prototype to `def.h`
```c
// printf.c
...
void            backtrace();
```

Then, add the following function to `riscv.h`, so that we can get the value in the register `fp`:
```c
#ifndef __ASSEMBLER__
...
static inline uint64 r_fp() {
  uint64 x;
  asm volatile("mv %0, s0" : "=r" (x) );
  return x;
}

typedef uint64 pte_t;
typedef uint64 *pagetable_t; // 512 PTEs

#endif // __ASSEMBLER__
```

From the hints:
> Note that the return address lives at a fixed offset (-8) from the frame pointer of a stackframe, and that the saved frame pointer lives at fixed offset (-16) from the frame pointer.

We can directly implement our `backtrace` in `kernel/printf.c` like the following:
```c
void backtrace(){
  uint64 sf_ptr = r_fp();
  uint64 pgdown = PGROUNDDOWN(sf_ptr);
  uint64 npgdown = pgdown;
  uint64 ra_ptr;

  printf("backtrace: \n");
  while(npgdown == pgdown){
    ra_ptr = sf_ptr - 8;
    sf_ptr = *(uint64*) (sf_ptr -16);
    npgdown = PGROUNDDOWN(sf_ptr);
    printf("%p\n",*(uint64*)(ra_ptr));
  }
  return;
}
```

Notice that we use `while(npgdown == pgdown)` to recognize the end of the stack trace. This is because all the stack frames reside on the same page.

Then we add a call to `backtrace()` in `sys_sleep` and `panic` to test our implementation:
```c
// kernel/sysproc.c
uint64 sys_sleep(void){
  backtrace();
  ...
}

// kernel/printf.c
void
panic(char *s)
{
  ...
  backtrace();
  for(;;)
    ;
}
```

## Alarm
We will implement `sigalarm` and `sigreturn` system calls to support **Alarm** function.

First, register these 2 syscalls as described in [lab:syscall](./syscall.md).

To accomplish the **Alarm**, our `struct proc` need to _remember_ something. So we add new fields to it:
```c
struct proc {
  ...
  // * New fields for alarm
  int ticks; // alarm interval
  int cnt; // Counter
  uint64 handler; // handler address
  struct trapframe *hdlerFrame; // Store the states:back-up the trapframe
  int handler_ret; // if handler not returned,shouldn't call handler again
};
```

These new fields should be allocated/initialized in `allocproc` and freed in `freeproc`. 

```c
allocproc(void)
{
  ...
found:
  ...
  // * alarm syscall field
  p->ticks = 0;
  p->cnt = 0;
  p->handler_ret = 1;
  // * Allocate a hdler trapframe to recover
  if((p->hdlerFrame = (struct trapframe *)kalloc()) == 0){
    freeproc(p);
    release(&p->lock);
    return 0;
  }
  ...
}

static void
freeproc(struct proc *p)
{
  if(p->trapframe)
    kfree((void*)p->trapframe);
  p->trapframe = 0;
  if(p->hdlerFrame)
    kfree((void*)p->hdlerFrame);
  p->hdlerFrame = 0;
  ...
}
```

In `sys_sigalarm`, we set the ticks and install the handler:
```c
uint64
sys_sigalarm(void){
  struct proc* p = myproc();
  argint(0,&(p->ticks));
  // * install the handler in the process
  argaddr(1,&(p->handler));
  return 0;
}
```

Then when the timer interrupt occurs, we should call the handler if the counter reach the ticks:
```c
void
usertrap(void){
  ...
  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2){
    if (p->ticks == 0) {
      yield();
    } else {
      // ! Alarm
      p->cnt += 1;
      if (p->cnt >= p->ticks && p->handler_ret){
        // - Wait until return
        p->handler_ret = 0;
        // - reset the counter
        p->cnt = 0; 
        // - Store the registers and pc
        memmove(p->hdlerFrame,p->trapframe,sizeof(struct trapframe));
        p->trapframe->epc = p->handler;
      }
    }
  }

  usertrapret();
}
```

In `sys_sigreturn`, we restore the registers and pc:
```c
uint64
sys_sigreturn(void){
  struct proc* p = myproc();
  memmove(p->trapframe,p->hdlerFrame,sizeof(struct trapframe));
  p->handler_ret = 1;
  return p->trapframe->a0;
}
```

## Test
run `make grade` to test your code. Don't forget to add a `time.txt` to get a full score.