#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "kernel/sysinfo.h"     //lab2
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

//lab2 trace 系统调用的具体实现
uint64 sys_trace(void)
{
  struct proc* p = myproc();
  int mask;
  if (argint(0, &mask) < 0)     //获取用户空间的传入参数
    return -1;
  for (int i = 0; i < 23; i++)    //获取每一个标志位
  {
    if ((mask >> i) & 0x01)
    {
      p->mask[i] = '1';
    } else {
      p->mask[i] = '0';
    }
  }
  return 0;
}

//lab2 sysinfo
uint64 sys_sysinfo(void)
{
  struct proc* p = myproc();
  struct sysinfo info;
  uint64 user_addr;
  if (argaddr(0,&user_addr) < 0)    //获取用户空间info的地址
    return -1;
  info.freemem = get_free_memory();
  info.nproc = get_proc_num();
  if (copyout(p->pagetable, user_addr, (char*)&info, sizeof(info)) < 0)  //user_addr用p->trapframe->a0也可以
  {
    return -1;
  } else {
    return 0;
  }
}