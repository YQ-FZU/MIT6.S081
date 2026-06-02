// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);
void* get_free_page(int id);
extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU];   //lab8 为每一个cpu都申请一张链表和锁

void
kinit()
{
  //lab8 初始化每个cpu的锁
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&kmem[i].lock, "kmem");
  }
 
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void
kfree(void *pa)
{
  struct run *r;

  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);

  r = (struct run*)pa;
  //lab8
  push_off();  //关中断
  int id = cpuid();     //获取当前cpuid
  acquire(&kmem[id].lock);
  r->next = kmem[id].freelist;
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off();    //开中断
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;
  //lab8
  push_off();   //关中断
  int id = cpuid();
  acquire(&kmem[id].lock);
  r = kmem[id].freelist;
  if (r)    
  {
    //如果cpu自身有内存
    kmem[id].freelist = r->next;
  }
  else
  {
    r = (struct run*)get_free_page(id);     //返回空闲页物理地址
    /*
      if(r)
    {
      r->next = kmem[id].freelist;
      kmem[id].freelist = r;
    }
    */
  }
  release(&kmem[id].lock);
  pop_off();    //关中断，只有id使用完之后才能关闭中断
  if(r)
    memset((char*)r, 5, PGSIZE); // fill with junk
  return (void*)r;
}
//lab8 寻找空闲页
void* get_free_page(int id)
{
  for (int i = 0; i < NCPU; i++)
  {
    //遍历所有的cpu查看存在空闲页的cpu
    if (i == id)
    {
      continue;   //防止死锁
    }
    acquire(&kmem[i].lock);
    struct run* r = kmem[i].freelist;
    if (r)    //如果有空闲内存
    {
      kmem[i].freelist = r->next;   //将该结点移除
      release(&kmem[i].lock);
      return (void*)r;
    }
    release(&kmem[i].lock);
  }
  //没有空闲内存
  return 0;
}