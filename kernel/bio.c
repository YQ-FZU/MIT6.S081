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
//lab 8
#define NBUCKET 13    //13个散列桶
struct bucket {
  struct spinlock lock;   //散列桶的锁
  struct buf head;        //散列桶头节点
};
struct {
  struct spinlock lock;     //全局锁
  struct buf buf[NBUF];    //缓冲块总数
  struct bucket buckets[NBUCKET];     //散列桶
} bcache;   //lab8 哈希表

uint global_time = 0;   //lab8 全局时间戳

//lab8 初始化
void binit(void)
{
  struct buf* b;
  //char lockname[16];
  initlock(&bcache.lock, "bcache");   //初始化全局锁
  for(int i = 0; i < NBUCKET; ++i) 
  {
    // 初始化散列桶的自旋锁
    //snprintf(lockname, sizeof(lockname), "bcache_%d", i);
    initlock(&bcache.buckets[i].lock, "bcache");

    // 初始化散列桶的头节点
    bcache.buckets[i].head.prev = &bcache.buckets[i].head;
    bcache.buckets[i].head.next = &bcache.buckets[i].head;
  }

  //初始化缓存块,并将缓存块全部放到散列桶0上
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    b->next = bcache.buckets[0].head.next;
    b->prev = &bcache.buckets[0].head;
    initsleeplock(&b->lock, "buffer");
    b->timestamp = 0;
    bcache.buckets[0].head.next->prev = b;
    bcache.buckets[0].head.next = b;
  }
  
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf*
bget(uint dev, uint blockno)
{
  //lab8
  struct buf *b;
  uint bucket_id = blockno % NBUCKET;   //获取块编号对应的散列桶id
  acquire(&bcache.buckets[bucket_id].lock);     //获取对应散列桶的锁

  //寻找磁盘块是否已经被映射到对应散列桶缓存块上了
  for (b = bcache.buckets[bucket_id].head.next; b != &bcache.buckets[bucket_id].head; b = b->next)
  {
    if (b->dev == dev && b->blockno == blockno)
    {
      ++b->refcnt;
      b->timestamp = __sync_fetch_and_add(&global_time, 1);  //更新时间戳
      release(&bcache.buckets[bucket_id].lock);   //释放散列桶的锁
      acquiresleep(&b->lock);           //加buffer的锁
      return b;
    }
  }
  release(&bcache.buckets[bucket_id].lock);     //没用的话也要解锁

  acquire(&bcache.lock);    //加全局锁，保证分配或者偷取缓存块是原子操作
                            //并且注意需要先加全局锁再加散列桶的锁
  //没有找到就去创建缓存块
  int i, cur_bucket, min_bucket_id = 0;
  struct buf* temp = 0; 
  b = 0;    //注意把b清零
  for (i = 0, cur_bucket = bucket_id; i < NBUCKET; i++, cur_bucket++)   //循环遍历所有散列桶,得到全局最小的lru
  {
    if (cur_bucket == NBUCKET)
    {
      cur_bucket = 0;
    }
    acquire(&bcache.buckets[cur_bucket].lock);  //给每个散列桶加锁

    for (temp = bcache.buckets[cur_bucket].head.next; temp != &bcache.buckets[cur_bucket].head; temp = temp->next)
    {
      if (temp->refcnt == 0 && (b == 0 || temp->timestamp < b->timestamp))
      {
        //这里判断b == 0是为了先不管时间，有空的先拿，然后再去看时间戳是否需要替换
        b = temp;
        min_bucket_id = cur_bucket;     //找到最合适buffer的桶标记
      }
      //找到当前散列桶里面refcnt = 0，且最久没用使用的
    }
    release(&bcache.buckets[cur_bucket].lock);
  }

  acquire(&bcache.buckets[min_bucket_id].lock);   //加桶的锁，保证先全局后桶
  if (b)    //如果全局缓存区还要空闲缓存块
  {
    if (min_bucket_id == bucket_id)     //是从目标散列桶里面拿的
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->timestamp = __sync_fetch_and_add(&global_time, 1);
      release(&bcache.buckets[min_bucket_id].lock);     //释放目标散列桶的锁的
      release(&bcache.lock);    //释放全局锁
      acquiresleep(&b->lock);         //加buffer的锁
      return b;
    }
    else    //从其他散列桶偷来的，将他插入目标桶
    {
      //将结点从其他的散列桶链表中断开
      b->next->prev = b->prev;
      b->prev->next = b->next;
      release(&bcache.buckets[min_bucket_id].lock);    //释放其他散列桶的锁
      //插入目标散列桶
      acquire(&bcache.buckets[bucket_id].lock);     //加目标散列通的锁
      b->next = bcache.buckets[bucket_id].head.next;
      b->prev = &bcache.buckets[bucket_id].head;
      bcache.buckets[bucket_id].head.next->prev = b;
      bcache.buckets[bucket_id].head.next = b; 

      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1;
      b->timestamp = __sync_fetch_and_add(&global_time, 1);
      release(&bcache.buckets[bucket_id].lock);     //释放目标散列桶的锁
      release(&bcache.lock);    //释放全局锁
      acquiresleep(&b->lock);         //加buffer的锁
      return b;
    }
  }
  else
  {
    //没用内存了
    panic("bget: no buffers");
  }
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
  //lab 8
  uint bucket_id = b->blockno % NBUCKET;
  acquire(&bcache.buckets[bucket_id].lock);     //lab8 获取散列桶的锁
  b->refcnt--;
  release(&bcache.buckets[bucket_id].lock);     //lab8
}

void
bpin(struct buf *b) {
  //lab8
  uint bucket_id = b->blockno % NBUCKET;
  acquire(&bcache.buckets[bucket_id].lock);    
  b->refcnt++;
  release(&bcache.buckets[bucket_id].lock);
}

void
bunpin(struct buf *b) {
  //lab8
  uint bucket_id = b->blockno % NBUCKET;
  acquire(&bcache.buckets[bucket_id].lock);
  b->refcnt--;
  release(&bcache.buckets[bucket_id].lock);
}


