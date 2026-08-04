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

#define BUCKET_NUM 13

struct HashTableNode
{
  struct buf data;
  struct HashTableNode *next;
};

struct Bucket
{
  struct spinlock lock;
  struct HashTableNode *head;
};

struct
{
  struct spinlock lock;
  struct buf buf[NBUF];

  // the hash table
  struct Bucket hash_table[BUCKET_NUM];
  // creating the nodes statically
  struct HashTableNode buf_nodes[NBUF];
} bcache;

uint hash(uint dev, uint blockno)
{
  return (dev * 31 + blockno) % BUCKET_NUM;
}

void binit(void)
{
  for (int i = 0; i < BUCKET_NUM; i++)
  {
    initlock(&bcache.hash_table[i].lock, "bcache_bucket");
    bcache.hash_table[i].head = 0;
  }

  // distributing the blocks evenly
  for (int i = 0; i < NBUF; i++)
  {
    bcache.buf_nodes[i].next = bcache.hash_table[i % BUCKET_NUM].head;
    bcache.hash_table[i % BUCKET_NUM].head = &bcache.buf_nodes[i];
  }

  // old code
  initlock(&bcache.lock, "bcache");
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{
  // struct buf *b;

  uint idx = hash(dev, blockno);
  struct Bucket *bucket = &bcache.hash_table[idx];
  struct HashTableNode *data = 0;
  struct HashTableNode **p = 0;
  // acquiring the bucket lock
  acquire(&bucket->lock);

  // iterating until we find our block or we reach the end
  data = bucket->head;
  while (data)
  {
    if (data->data.dev == dev && data->data.blockno == blockno)
    {
      // we found the block
      break;
    }
    data = data->next;
  }
  if (data)
  {
    // block was found
    data->data.refcnt++;
    release(&bucket->lock);
    acquiresleep(&data->data.lock);
    return &data->data;
  }
  // block was not found
  release(&bucket->lock); // releasing while we are looking in different buckets
  // searching the hash table
  for (int i = 0; i < BUCKET_NUM; i++)
  {
    acquire(&bcache.hash_table[i].lock);
    p = &bcache.hash_table[i].head;
    data = bcache.hash_table[i].head;
    // looking for a buf with 0 refcnt
    while (data)
    {
      if (data->data.refcnt == 0)
      {
        // removing node from bucket
        *p = data->next;
        data->next = 0;
        break;
      }
      p = &data->next;
      data = data->next;
    }
    release(&bcache.hash_table[i].lock);
    if (data)
    {
      break;
    }
  }

  if (data)
  {
    // inserting to bucket
    acquire(&bucket->lock);
    data->next = bucket->head;
    bucket->head = data;
    data->data.dev = dev;
    data->data.blockno = blockno;
    data->data.valid = 0;
    data->data.refcnt = 1;
    release(&bucket->lock);
    acquiresleep(&data->data.lock);
    return &data->data;
  }

  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);

  uint idx = hash(b->dev, b->blockno);
  struct Bucket *bucket = &bcache.hash_table[idx];

  acquire(&bucket->lock);

  b->refcnt--;

  release(&bucket->lock);
}

void bpin(struct buf *b)
{
  acquire(&bcache.lock);
  b->refcnt++;
  release(&bcache.lock);
}

void bunpin(struct buf *b)
{
  acquire(&bcache.lock);
  b->refcnt--;
  release(&bcache.lock);
}
