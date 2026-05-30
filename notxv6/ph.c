#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>

#define NBUCKET 5   //哈希表桶数
#define NKEYS 100000    //总键数

struct entry {    //哈希表结点（链表）
  int key;
  int value;
  struct entry *next;
};
struct entry *table[NBUCKET];     //哈希表
int keys[NKEYS];      // 存储所有随机生成的键
int nthread = 1;      // 线程数（通过命令行参数指定）

//pthread_mutex_t lock;   //lab7 ph_safe 申请一个锁
pthread_mutex_t lock[NBUCKET];   // lab7 ph_fast 给每一个散列桶都申请一个锁

double
now()     //获取当前时间
{
 struct timeval tv;
 gettimeofday(&tv, 0);
 return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// 功能：向哈希表的指定链表头部插入新节点
// 参数：
//   key - 要插入的键
//   value - 要插入的值
//   p - 链表头指针的地址（用于修改头指针）
//   n - 原链表头指针（新节点的next指向它
static void 
insert(int key, int value, struct entry **p, struct entry *n)
{
  struct entry *e = malloc(sizeof(struct entry));
  e->key = key;
  e->value = value;
  e->next = n;
  *p = e;     // 更新链表头为新节点（头插法）
}
// 功能：哈希表插入/更新操作
// 逻辑：先查询键是否存在，存在则更新值，不存在则调用insert插入新节点
// 参数：
//   key - 要插入/更新的键
//   value - 对应的值
static 
void put(int key, int value)
{
  int i = key % NBUCKET;  // 计算键对应的哈希桶索引（取模运算

  // is the key already present?
  // 第一步：遍历桶i的链表，检查键是否已存在
  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key)    //找到键值，跳出循环
      break;
  }
  if(e){
    // update the existing key.
    e->value = value;     //更新键值
  } else {
    // the new is new.
    /*
    //lab7 ph_safe 解决数据丢失
    pthread_mutex_lock(&lock); // 插入前先加锁<解决数据丢失>
    insert(key, value, &table[i], table[i]);      //如果没有找到那么就插入（头插）
    pthread_mutex_unlock(&lock);    //解锁<解决数据丢失>
    */

      //lab7 ph_fast 解决数据丢失
    pthread_mutex_lock(&lock[i]); // 插入前先加锁<解决数据丢失>
    insert(key, value, &table[i], table[i]);      //如果没有找到那么就插入（头插）
    pthread_mutex_unlock(&lock[i]);    //解锁<解决数据丢失>
    
   
  }
}
// 功能：哈希表查询操作
// 参数：key - 要查询的键
// 返回值：找到则返回对应节点指针，否则返回NULL
static struct entry*
get(int key)
{
  int i = key % NBUCKET;


  struct entry *e = 0;
  for (e = table[i]; e != 0; e = e->next) {
    if (e->key == key) break;
  }

  return e;
}
// 功能：PUT线程函数（每个线程执行部分键的put操作）
// 参数：xa - 线程编号（void*类型，需强转）
// 返回值：NULL（符合pthread线程函数要求）
static void *
put_thread(void *xa)
{
  // 将void*类型的参数强转为线程编号（long中转避免类型警告）
  int n = (int) (long) xa; // thread number
  int b = NKEYS/nthread;     // 计算每个线程需要处理的键数量（总键数均分）

  for (int i = 0; i < b; i++) {
    put(keys[b*n + i], n);    // 键：按线程分区取keys数组元素；值：线程编号
  }

  return NULL;
}
// 功能：GET线程函数（每个线程遍历所有键执行get操作，统计缺失数）
// 参数：xa - 线程编号（void*类型，需强转）
// 返回值：NULL（符合pthread线程函数要求）
static void *
get_thread(void *xa)
{
  int n = (int) (long) xa; // thread number
  int missing = 0;      // 统计查询不到的键的数量

  for (int i = 0; i < NKEYS; i++) {
    struct entry *e = get(keys[i]);
    if (e == 0) missing++;    // 未找到键，缺失数+1
  }
  printf("%d: %d keys missing\n", n, missing);    // 打印当前线程的缺失键数量
  return NULL;  
}

int
main(int argc, char *argv[])
{
  pthread_t *tha;
  void *value;
  double t1, t0;

  if (argc < 2) {
    fprintf(stderr, "Usage: %s nthreads\n", argv[0]);
    exit(-1);
  }

  //pthread_mutex_init(&lock, NULL);    //lab7 ph_safe 初始化锁
  //lab7 ph_fast 初始化每个锁
    for (int j = 0; j < NBUCKET; j++)
  {
    pthread_mutex_init(&lock[j], NULL);
  }
  
  

  nthread = atoi(argv[1]);
  tha = malloc(sizeof(pthread_t) * nthread);
  srandom(0);
  assert(NKEYS % nthread == 0);
  for (int i = 0; i < NKEYS; i++) {     //生成n个随机数存入数组
    keys[i] = random();
  }

  //
  // first the puts
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {    //床架n个线程用来插入
    assert(pthread_create(&tha[i], NULL, put_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {      //阻塞等待线程插入完成释放线程
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d puts, %.3f seconds, %.0f puts/second\n",
         NKEYS, t1 - t0, NKEYS / (t1 - t0));

  //
  // now the gets
  //
  t0 = now();
  for(int i = 0; i < nthread; i++) {
    assert(pthread_create(&tha[i], NULL, get_thread, (void *) (long) i) == 0);
  }
  for(int i = 0; i < nthread; i++) {
    assert(pthread_join(tha[i], &value) == 0);
  }
  t1 = now();

  printf("%d gets, %.3f seconds, %.0f gets/second\n",
         NKEYS*nthread, t1 - t0, (NKEYS*nthread) / (t1 - t0));
}
