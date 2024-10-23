// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.


#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"

// struct {
//   struct spinlock lock;
//   struct buf buf[NBUF];

//   // Linked list of all buffers, through prev/next.
//   // Sorted by how recently the buffer was used.
//   // head.next is most recent, head.prev is least.
//   struct buf head;
// } bcache;
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

void
binit(void)
{
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // // Create linked list of buffers
  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  // for(b = bcache.buf; b < bcache.buf+NBUF; b++){
  //   b->next = bcache.head.next;
  //   b->prev = &bcache.head;
  //   initsleeplock(&b->lock, "buffer");
  //   bcache.head.next->prev = b;
  //   bcache.head.next = b;
  // }
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

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
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

// Return a locked buf with the contents of the indicated block.
struct buf*
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if(!b->valid) {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void
bwrite(struct buf *b)
{
  if(!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
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

void
bpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void
bunpin(struct buf *b) {
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}


