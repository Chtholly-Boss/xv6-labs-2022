# Lock
[MIT6.1810 Lock Page](https://pdos.csail.mit.edu/6.S081/2022/labs/lock.html)

This lab teaches you how to loose contention on shared resources by replacing a global lock with several locks.

The idea is to decrease granularity of the lock.

## Memory Allocator
The allocator is a global lock, which is not efficient.

The solution is to use a lock per CPU.

We will finish this task in `kalloc.c` only.

First, modify the `kmem` structure to hold a lock for each CPU.
```c
struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU]; // - kmem per cpu
```

In `kinit`, initialize the locks.
```c
void
kinit()
{
  // * kmem per cpu
  for(int i = 0;i < NCPU; i++){
    initlock(&kmem[i].lock,"kmem");
  }
  // * the cpu running freerange will get all the free memory
  freerange(end, (void*)PHYSTOP);
}
```

In `kalloc`, steal memory from other CPUs if the free list of the current CPU is empty.Otherwise, allocate memory as usual.
```c
void *
kalloc(void)
{
  // ! disable interrupt
  push_off();
  int id = cpuid();
  pop_off();

  struct run *r;

  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if(r) {
    kmem[id].freelist = r->next;
    release(&kmem[id].lock);
  } else {
    release(&kmem[id].lock);
    // - free-list empty, steal others
    for(int i = 0;i < NCPU; i++){
      acquire(&kmem[i].lock);
      r = kmem[i].freelist;
      if(r){
        kmem[i].freelist = r->next;
        release(&kmem[i].lock);
        break;
      } else {
        release(&kmem[i].lock);
      }
    }
  }

  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
```

In `kfree`, add the freed page to the free list of the current CPU.
```c
void
kfree(void *pa)
{
  // ! disable interrupt
  push_off();
  int id = cpuid();
  pop_off();

  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // * Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;

  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
}
```

## Buffer Cache
The buffer cache is a global lock, which is not efficient.

Split the bcache into buckets and hash the `blockno` into them.

We will finish this task in `bio.c` only.

First, modify the `bcache` structure to hold a lock for each bucket.
```c
// * split the bcache into buckets
#define BUCKETS 13
struct bucket {
  struct spinlock lock;
  struct buf head;
};

struct {
  struct spinlock lock;
  struct buf buf[NBUF];
  struct bucket buckets[BUCKETS];
} bcache;
```

Then, in `binit`, initialize the locks.
```c
void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");
  // * init the buckets
  for(int i = 0; i < BUCKETS; i++){
    b = &bcache.buckets[i].head;
    b->next = b;
    b->prev = b;
    initlock(&bcache.buckets[i].lock,"bcache");
  }
  // * add all buf to bucket 0
  b = &bcache.buckets[0].head;
  // * add all bufs to the freelist
  for(int i = 0; i < NBUF; i++){
    initsleeplock(&bcache.buf->lock,"buffer");
    bcache.buf[i].next = b->next;
    b->next->prev = &bcache.buf[i];

    b->next = &bcache.buf[i];
    bcache.buf[i].prev = b;
  }
}
```

In `bget`, when the bucket is empty, steal from other buckets.
```c
static struct buf*
bget(uint dev, uint blockno)
{
  struct buf *b;
  struct buf *h;
  // - check the bucket
  int idx = blockno % BUCKETS;
  acquire(&bcache.buckets[idx].lock);
  h = &bcache.buckets[idx].head;
  b = h->next;
  while(b != h){
    if(b->blockno == blockno && b->dev == dev){
      // - buf cached in the bucket
      b->refcnt++;
      goto getBuf;   
    }
    b = b->next;
  }
  // ! not in the bucket
  // !!! You should find in itself first
  // !!! Otherwise, when two cpu request for the same block, the bucket will have 
  // !!! different blocks for the same block node
  b = h->next;
  while (b != h)  
  {
    if (b->refcnt == 0) {
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        goto getBuf;
    }
    b = b->next;
  }
  release(&bcache.buckets[idx].lock);
  // - check bucket by bucket to find a unused
  for(int i = 0; i < BUCKETS; i++){
    if (i == idx)
      continue;
    acquire(&bcache.buckets[i].lock);
    h = &bcache.buckets[i].head;
    b = h->next;
    while(b != h){
      if(b->refcnt == 0){
        // - get it from the bucket
        b->dev = dev;
        b->blockno = blockno;
        b->valid = 0;
        b->refcnt = 1;
        b->prev->next = b->next;
        b->next->prev = b->prev;
        release(&bcache.buckets[i].lock);
        goto mount;
      }
      b = b->next;
    }
    release(&bcache.buckets[i].lock);
  }
  panic("bget: no buffers");

mount:
  acquire(&bcache.buckets[idx].lock);
  h = &bcache.buckets[idx].head;
  b->next = h->next;
  h->next->prev = b;
  h->next = b;
  b->prev = h;
getBuf:
  release(&bcache.buckets[idx].lock);
  acquiresleep(&b->lock);
  return b;
}
```

In `brelse`, when the buffer is not used, keep it in the bucket, just need to mark the `refcnt` as 0.
```c
void
brelse(struct buf *b)
{
  if(!holdingsleep(&b->lock))
  panic("brelse");

  releasesleep(&b->lock);

  int idx = b->blockno % BUCKETS;
  acquire(&bcache.buckets[idx].lock);
  b->refcnt--;
  release(&bcache.buckets[idx].lock);
}
```

## Test
run `make grade` to test your code. Don't forget to add a `time.txt` to get a full score.