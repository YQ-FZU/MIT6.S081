#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  if(argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if(argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if(argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if(argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(myproc()->killed){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  backtrace();    //调用回溯
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if(argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

//lab4
uint64 sys_sigalarm(void)
{
  if (argint(0, &myproc()->alarm_inteval) < 0)   //传入的第一个参数是触发定时器中断的时间间隔
    return -1;
  if (argaddr(1, &myproc()->handler) < 0)    
    return -1;
  return 0;
}

uint64 sys_sigreturn(void)
{
  //用户中断服务程序在结束的时候会调用sigreturn系统调用，这个时候ecall会把sigreturn的现场写道sepc
  //进入trap之后，usertrap会把sepc+4写入陷阱帧用于userret的sret返回
  //而我们需要返回到触发警报之前的现场，所以我们需要修改陷阱帧的sepc,alarm_trapframe存储的就是触发警报之前的现场
  
  struct proc *p = myproc();
  memmove(p->trapframe, p->alarm_trapframe, sizeof(struct trapframe));
  p->cnt = 0;
  p->in_hanlder = 0;
  return 0;
}