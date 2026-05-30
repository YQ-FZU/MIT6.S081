#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

/* Possible states of a thread: */
#define FREE        0x0
#define RUNNING     0x1
#define RUNNABLE    0x2

#define STACK_SIZE  8192
#define MAX_THREAD  4

struct context {    //lab7
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};
struct thread {
  char       stack[STACK_SIZE]; /* the thread's stack */
  int        state;             /* FREE, RUNNING, RUNNABLE */
  struct context context;       //lab7 模仿进程，我们也定义一个上下文
};
struct thread all_thread[MAX_THREAD];
struct thread *current_thread;
extern void thread_switch(uint64, uint64);
              
void 
thread_init(void)
{
  // main() is thread 0, which will make the first invocation to
  // thread_schedule().  it needs a stack so that the first thread_switch() can
  // save thread 0's state.  thread_schedule() won't run the main thread ever
  // again, because its state is set to RUNNING, and thread_schedule() selects
  // a RUNNABLE thread.
  current_thread = &all_thread[0];
  current_thread->state = RUNNING;
}

void 
thread_schedule(void)
{
  struct thread *t, *next_thread;   //用t来遍历所有数组，用next_thread表示下一个要切换的线程

  /* Find another runnable thread. */
  next_thread = 0;
  t = current_thread + 1;       //将t标记为当前线程的下一个线程
  for(int i = 0; i < MAX_THREAD; i++){    //只遍历一轮
    if(t >= all_thread + MAX_THREAD)
      t = all_thread;             //环形遍历，遍历到数组尾回到头部继续遍历
    if(t->state == RUNNABLE) {    //找到处于runnable状态的线程，标记为下一个执行的线程
      next_thread = t;
      break;                      //找到了就break
    }
    t = t + 1;                  //t指向下一个
  }

  if (next_thread == 0) {       //如果没有可以切换的线程，出错
    printf("thread_schedule: no runnable threads\n");
    exit(-1);
  }

  if (current_thread != next_thread) {        //如果下一个线程不是当前线程（yield会把当前线程的state标记为runable）
    next_thread->state = RUNNING;             //设置下一个线程为running
    t = current_thread;                       //零时存储当前线程
    current_thread = next_thread;             //将当前线程切换为下一个需要运行的线程
    /* YOUR CODE HERE
     * Invoke thread_switch to switch from t to next_thread:
     * thread_switch(??, ??);
     */
    thread_switch((uint64)&t->context, (uint64)&next_thread->context);    //lab7
  } else    //如果下一个线程是当前线程则不需要切换
    next_thread = 0;    
}

void 
thread_create(void (*func)())
{
  struct thread *t;

  for (t = all_thread; t < all_thread + MAX_THREAD; t++) {    //寻找线程坑位
    if (t->state == FREE) break;
  }
  t->state = RUNNABLE;        //设置为可运行
  // YOUR CODE HERE
  //lab7
  memset(&t->context, 0, sizeof(t->context));   //先清理内存防止读取到脏数据
  t->context.ra = (uint64)func;     //设置ra为回调函数的地址
  t->context.sp = (uint64)t->stack + STACK_SIZE;    //栈是自顶向下生长的
}

void 
thread_yield(void)
{
  current_thread->state = RUNNABLE;
  thread_schedule();
}

volatile int a_started, b_started, c_started;
volatile int a_n, b_n, c_n;

void 
thread_a(void)
{
  int i;
  printf("thread_a started\n");
  a_started = 1;
  while(b_started == 0 || c_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_a %d\n", i);
    a_n += 1;
    thread_yield();
  }
  printf("thread_a: exit after %d\n", a_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_b(void)
{
  int i;
  printf("thread_b started\n");
  b_started = 1;
  while(a_started == 0 || c_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_b %d\n", i);
    b_n += 1;
    thread_yield();
  }
  printf("thread_b: exit after %d\n", b_n);

  current_thread->state = FREE;
  thread_schedule();
}

void 
thread_c(void)
{
  int i;
  printf("thread_c started\n");
  c_started = 1;
  while(a_started == 0 || b_started == 0)
    thread_yield();
  
  for (i = 0; i < 100; i++) {
    printf("thread_c %d\n", i);
    c_n += 1;
    thread_yield();
  }
  printf("thread_c: exit after %d\n", c_n);

  current_thread->state = FREE;
  thread_schedule();
}

int 
main(int argc, char *argv[]) 
{
  a_started = b_started = c_started = 0;
  a_n = b_n = c_n = 0;
  thread_init();
  thread_create(thread_a);
  thread_create(thread_b);
  thread_create(thread_c);
  thread_schedule();
  exit(0);
}
