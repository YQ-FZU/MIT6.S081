// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

//lab6 定义一个引用计数指针用于标记每个页被多少个进程占用
struct ref_cnt{ 
  struct spinlock lock;
  int cnt[PHYSTOP / PGSIZE];  
}ref;


int refcnt_dec(void* pa);
void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run {
  struct run *next;
};

struct {
  struct spinlock lock;
  struct run *freelist;
} kmem;

void
kinit()
{
  initlock(&kmem.lock, "kmem");
  initlock(&ref.lock, "ref");   //lab6 初始化自旋锁
  freerange(end, (void*)PHYSTOP);
}

void
freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char*)PGROUNDUP((uint64)pa_start);
  for(; p + PGSIZE <= (char*)pa_end; p += PGSIZE)
  {
    acquire(&ref.lock);
    ref.cnt[(uint64)p / PGSIZE] = 1;    //lab6 需要将计数指针置为1不然kfree删除不掉。
    release(&ref.lock);
    kfree(p);
  }
    
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
  //lab6
  acquire(&ref.lock);
  if (--ref.cnt[(uint64)pa / PGSIZE] == 0)  //lab5，如果计数索引为0则释放物理内存
  {
    release(&ref.lock);
    // Fill with junk to catch dangling refs.
    memset(pa, 1, PGSIZE);

    r = (struct run*)pa;

    acquire(&kmem.lock);
    r->next = kmem.freelist;
    kmem.freelist = r;
    release(&kmem.lock);
  }
  else
  {
    release(&ref.lock);
  }
  
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{
  struct run *r;

  acquire(&kmem.lock);
  r = kmem.freelist;
  if(r)
  {
    acquire(&ref.lock);
    ref.cnt[(uint64)r / PGSIZE] = 1;  //与这块物理内存相关联的进程只有1个
    release(&ref.lock);
    kmem.freelist = r->next;
  }
  release(&kmem.lock);

  if(r)
  {
    memset((char*)r, 5, PGSIZE); // fill with junk
  }
    
  return (void*)r;
}
//判断是否为cow页
int cowpage(pagetable_t pagetable, uint64 va)
{
  if (va > MAXVA)
    return -1;
  pte_t* pte = walk(pagetable, va, 0);
  if (pte == 0)
    return -1;
  if ((*pte & PTE_V) == 0)
    return -1;
  return ((*pte & PTE_COW) ? 0 : -1);
}
//获取当前页计数指针值
int get_refcnt(void* pa)
{
  return ref.cnt[(uint64)pa / PGSIZE];
}
//当前页计数指针++
int refcnt_add(void* pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  acquire(&ref.lock);
  ++ref.cnt[(uint64)pa /PGSIZE];
  release(&ref.lock);
  return 0;
}
//当前页计数指针--
int refcnt_dec(void* pa)
{
  if(((uint64)pa % PGSIZE) != 0 || (char*)pa < end || (uint64)pa >= PHYSTOP)
    return -1;
  acquire(&ref.lock);
  --ref.cnt[(uint64)pa /PGSIZE];
  release(&ref.lock);
  return 0;
}
/*
  cow页处理函数
  1、页计数指针为1
  这是由于假设父进程fork了一个子进程，这个页就会变成ref=2的cow页，那么子进程想往里面写数据
  这个时候会触发写时复制，cowalloc会匹配计数指针为2的情况为子进程单独开辟一块ref=1的页，然后
  原来cow页的ref就会减1，所以就出现了ref = 1的cow页；对于这类页我们只需要给他权限就行了
  2、对于页计数指针>=2的页
  说明这个页触发中断需要对其进行写时复制，所以我们需要为trap的进程开辟一个新页来存储写入的数据
  并将trap的进程的虚拟地址写入这个页
  3、函数返回值
  出错：返回0
  成功：返回传入虚拟地址映射的物理页地址
*/
void* cowalloc(pagetable_t pagetable, uint64 va)
{
  if (va % PGSIZE != 0)   //va是否页对齐
  {
    return 0;
  }
  uint64 pa = walkaddr(pagetable, va);    //得到物理页
  if (pa == 0)
    return 0;
  //计数指针为1的cow页，则对此页单独给与写权限
  pte_t* pte = walk(pagetable, va, 0);
  if (get_refcnt((void*)pa) == 1)
  {
    *pte |= PTE_W;
    *pte &= ~PTE_COW;
    return (char*)pa;
  }
  else
  {
    //如果计数指针>=2那么就要进行写时复制了
    char *mem = kalloc();
    if (mem == 0)
      return 0;
    memmove(mem, (char*)pa, PGSIZE);    //开辟一个新的物理页，并把原来物理页的内容复制过去
    //将当前进程的虚拟地址映射到这个物理页上去
    *pte &= ~PTE_V;     //避免报错重复映射
    if (mappages(pagetable, va, PGSIZE, (uint64)mem, (PTE_FLAGS(*pte) | PTE_W) & ~PTE_COW) != 0)
    {
      kfree(mem);
      *pte |= PTE_V;
      return 0;
    }
    kfree((char*)PGROUNDDOWN(pa));    //将原来物理页的计数指针--
    return mem;
  }
  
}