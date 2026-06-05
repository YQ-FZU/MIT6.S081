#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
//lab10
#include "file.h"

struct spinlock tickslock;
uint ticks;

extern char trampoline[], uservec[], userret[];

// in kernelvec.S, calls kerneltrap().
void kernelvec();

extern int devintr();

void
trapinit(void)
{
  initlock(&tickslock, "time");
}

// set up to take exceptions and traps while in the kernel.
void
trapinithart(void)
{
  w_stvec((uint64)kernelvec);
}

//
// handle an interrupt, exception, or system call from user space.
// called from trampoline.S
//
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
  
  if(r_scause() == 8){
    // system call

    if(p->killed)
      exit(-1);

    // sepc points to the ecall instruction,
    // but we want to return to the next instruction.
    p->trapframe->epc += 4;

    // an interrupt will change sstatus &c registers,
    // so don't enable until done with those registers.
    intr_on();

    syscall();
  } else if((which_dev = devintr()) != 0){
    // ok
  } 
  else if(r_scause() == 12 || r_scause() == 13 || r_scause() == 15)
  {
    //lab10
    uint64 va = r_stval();      //读取发生页错误的虚拟地址
    uint64 pa = 0;
    struct vam* vam = 0;
    struct inode* ip;
    int flags = 0;     //映射页权限
    int i, ret = 0;

    //找到va对应的vam
    for (i = 0; i < NVAM; i++)
    {
      if (va >= p->vam[i].addr && va < p->vam[i].addr + p->vam[i].length) //注意边界左闭右开
      {
        vam = &p->vam[i];
        break;
      }
    }
    if (!vam)     
    {
      //没找到vam
      printf("err vam\n");
      p->killed = 1;
      goto end;
    }

    //============处理脏页=========================
    if (r_scause() == 12 || r_scause() == 13)   //如果是读取和执行指令触发的页错误，原因是页未映射
    {
      ret = 1;          //对应第一种情况
    }
    else
    {
      ret = dirty_write(vam->port, va);
      if (ret == 2)
      {
        flags |= PTE_D;
      }
      else if (ret == 3)
      {
        //第三种情况是已经映射了，只需要给写权限并且标记为脏页就行，在dirty_write中处理不需要分配物理页
        goto end;
      }
    }
    //==============================================

    //分配物理页
    if ((pa = (uint64)kalloc()) != 0) 
    {
      //虚拟地址有效并且物理内存未耗尽  
      va = PGROUNDDOWN(va);     //页对齐
      memset((void*)pa, 0, PGSIZE);
    }
    else
    {
      p->killed = 1;
      goto end;
    }
    
    //把文件写入vam内存
    ip = vam->file->ip;
    ilock(ip);      //操作inode前需要加锁
    
    //一次最多读取一页文件数据到内存
    //文件和虚拟内存还有物理内存都是页对齐的
    //vam->offset对应vam->addr，va - vam->addr只可能是4096的整数倍
    if (readi(ip, 0, pa, vam->offset + (va - vam->addr), PGSIZE) < 0)
    {
      kfree((char*)pa);
      iunlock(ip);
      p->killed = 1;
      goto end;
    }
    iunlock(ip);

    //物理内存映射到虚拟地址
    flags = (PORT2PTE(vam->port) | PTE_U);    //设置映射页权限
     //处理脏页
    if (ret == 1)   //对应第一种情况，需要取消写标志位
    {
      flags &= (~PTE_W);
    }

    if (mappages(p->pagetable, va, PGSIZE, pa, flags) != 0)
    {
      kfree((char*)pa);
      p->killed = 1;
      goto end;
    }
    
  } else {
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    p->killed = 1;
  }
end:
  if(p->killed)
    exit(-1);

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2)
    yield();

  usertrapret();
}

//
// return to user space
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // we're about to switch the destination of traps from
  // kerneltrap() to usertrap(), so turn off interrupts until
  // we're back in user space, where usertrap() is correct.
  intr_off();

  // send syscalls, interrupts, and exceptions to trampoline.S
  w_stvec(TRAMPOLINE + (uservec - trampoline));

  // set up trapframe values that uservec will need when
  // the process next re-enters the kernel.
  p->trapframe->kernel_satp = r_satp();         // kernel page table
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // process's kernel stack
  p->trapframe->kernel_trap = (uint64)usertrap;
  p->trapframe->kernel_hartid = r_tp();         // hartid for cpuid()

  // set up the registers that trampoline.S's sret will use
  // to get to user space.
  
  // set S Previous Privilege mode to User.
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // clear SPP to 0 for user mode
  x |= SSTATUS_SPIE; // enable interrupts in user mode
  w_sstatus(x);

  // set S Exception Program Counter to the saved user pc.
  w_sepc(p->trapframe->epc);

  // tell trampoline.S the user page table to switch to.
  uint64 satp = MAKE_SATP(p->pagetable);

  // jump to trampoline.S at the top of memory, which 
  // switches to the user page table, restores user registers,
  // and switches to user mode with sret.
  uint64 fn = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64,uint64))fn)(TRAPFRAME, satp);
}

// interrupts and exceptions from kernel code go here via kernelvec,
// on whatever the current kernel stack is.
void 
kerneltrap()
{
  int which_dev = 0;
  uint64 sepc = r_sepc();
  uint64 sstatus = r_sstatus();
  uint64 scause = r_scause();
  
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  if((which_dev = devintr()) == 0){
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // give up the CPU if this is a timer interrupt.
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // the yield() may have caused some traps to occur,
  // so restore trap registers for use by kernelvec.S's sepc instruction.
  w_sepc(sepc);
  w_sstatus(sstatus);
}

void
clockintr()
{
  acquire(&tickslock);
  ticks++;
  wakeup(&ticks);
  release(&tickslock);
}

// check if it's an external interrupt or software interrupt,
// and handle it.
// returns 2 if timer interrupt,
// 1 if other device,
// 0 if not recognized.
int
devintr()
{
  uint64 scause = r_scause();

  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // this is a supervisor external interrupt, via PLIC.

    // irq indicates which device interrupted.
    int irq = plic_claim();

    if(irq == UART0_IRQ){
      uartintr();
    } else if(irq == VIRTIO0_IRQ){
      virtio_disk_intr();
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // the PLIC allows each device to raise at most one
    // interrupt at a time; tell the PLIC the device is
    // now allowed to interrupt again.
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // software interrupt from a machine-mode timer interrupt,
    // forwarded by timervec in kernelvec.S.

    if(cpuid() == 0){
      clockintr();
    }
    
    // acknowledge the software interrupt by clearing
    // the SSIP bit in sip.
    w_sip(r_sip() & ~2);

    return 2;
  } else {
    return 0;
  }
}

