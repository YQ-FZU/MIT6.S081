# lab1

## sleep

```c
#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        fprintf(2, "sleep error\n");
        exit(1);
    }
    sleep(atoi(argv[1]));       //将字符串转换为整数
    exit(0);
}

```

## pingpong

```c
#include "user/user.h"
#include "kernel/types.h"
#include "kernel/stat.h"
int main(int aegc, char* argv[])
{
    char buf[256];
    int pid;
    int fd_father_to_son[2];
    int fd_son_to_father[2];
    pipe(fd_father_to_son);
    pipe(fd_son_to_father);       //创建管道
    pid = fork();
    if (pid > 0)
    {
        //父进程
        //父进程发送ping
        close(fd_father_to_son[0]);       //关闭读端
        write(fd_father_to_son[1], "ping\n", strlen("ping\n"));
        close(fd_father_to_son[1]);

        //父进程接收pong
        close(fd_son_to_father[1]);     //关闭写端口
        read(fd_son_to_father[0], buf, strlen("pong\n"));
        close(fd_son_to_father[0]);
        printf("%d: received %s", getpid(), buf);    //父进程打印收到的数据和自身pid
        exit(0);

    }
    else if (pid == 0)
    {
        //子进程
        //子进程接收ping
        close(fd_father_to_son[1]);     //关闭写端口
        read(fd_father_to_son[0], buf, strlen("ping\n"));
        close(fd_father_to_son[0]);
        printf("%d: received %s", getpid(), buf);    //子进程打印收到的数据和自身pid

        //子进程发送pong
        close(fd_son_to_father[0]);       //关闭读端
        write(fd_son_to_father[1], "pong\n", strlen("pong\n"));
        close(fd_son_to_father[1]);
        exit(0);
    }
    else
    {
        fprintf(2,"fork error\n");
        exit(1);
    }
}
```

## primes

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char* argv[])
{
    int num[34];    //用于存储2~35
    int fd[2];
    int index = 0;
    for (int i = 2; i <= 35; i++)
    {
        num[index] = i;
        index++;
    }
    pipe(fd);
    int pid = fork();
    if (pid > 0)
    {
       close(fd[0]);
       for (int i = 0; i < 34; i++)
       {
            write(fd[1], &num[i], sizeof(num[i]));      //写入管道粒度尽可能小，避免出现读取错误
       }
       close(fd[1]);
       wait((int*)0);
       exit(0);

    }
    else if (pid == 0)
    {
        int temp;
        close(fd[1]);
        while (read(fd[0], &temp, sizeof(temp)) != 0)
        {
            int flag = 1;
            //判断是否为质数
            for (int i = 2; i < temp; i++)
            {
                if (temp == 2)
                {
                    flag = 1;
                    break;
                }
                else if (temp % i == 0)   //不是质数
                {
                    flag = 0;
                    break;
                }
            }
            if (flag == 1)
            {
                printf("prime %d\n", temp);
            }
            
        }
        close(fd[1]);
        exit(0);
    }
    else{
        fprintf(2, "fork error\n");
        exit(1);
    }
}
```

## find

```c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"


void find(char *path, char* target)
{
  char buf[512], *p;
  int fd;
  struct dirent de;
  struct stat st;

  
  if((fd = open(path, 0)) < 0){
    fprintf(2, "ls: cannot open %s\n", path);
    return;
  }

  if(fstat(fd, &st) < 0){
    fprintf(2, "ls: cannot stat %s\n", path);
    close(fd);
    return;
  }

  switch(st.type){
  case T_FILE:  //普通文件
    if (strcmp(path + strlen(path) - strlen(target), target) == 0)  //截取目标文件比较
    {
        printf("%s\n", path);
    }
    break;

  case T_DIR:
    if(strlen(path) + 1 + DIRSIZ + 1 > sizeof buf)//避免堆栈溢出：path+dirsize+/+\0
    {
      printf("ls: path too long\n");
      break;
    }
    strcpy(buf, path);          //将path拷贝到buf
    p = buf+strlen(buf);        //p指向buf的'\0'
    *p++ = '/';                 //将\0替换成/再p++
    while(read(fd, &de, sizeof(de)) == sizeof(de))
    {
      if(de.inum == 0)      //跳过无效目录项
      {
         continue;
      }
      if (strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0)
      {
        continue;
      }
      memmove(p, de.name, DIRSIZ);  //将文件名拼接到buf后
      p[DIRSIZ] = 0;                //在字符串末尾添加\0
      //到此buf已经拼接完成了       //./a/b/...
      find(buf, target);
     
      
    }
    break;
  }
  close(fd);
}

int main(int argc, char *argv[])
{
  if (argc < 3)
  {
    fprintf(2, "input error\n");
    exit(1);
  }
  find(argv[1], argv[2]);
  exit(0);
}

```

## xargs

```c
#include "kernel/types.h"
#include "user/user.h"
#include "kernel/stat.h"
#include "kernel/param.h"


int main(int argc, char* argv[])
{
    char stdin_buf[512];
    char *stdin_args[MAXARG]; // 存储从标准输入解析出的参数
    int stdin_argc = 0;

    // cmd_argv 存储 "目标命令 + 其固定参数"
    char *cmd_argv[MAXARG];
    int cmd_argc = 0;

    if (argc < 2) {
        // 默认命令是 echo
        cmd_argv[cmd_argc++] = "echo";
    } else {
        // 用户指定了命令，如 "xargs echo hello"
        for (int i = 1; i < argc; i++) {
            cmd_argv[cmd_argc++] = argv[i];
        }
    }
    cmd_argv[cmd_argc] = 0; // 确保 cmd_argv 以 NULL 结尾

    //读取标准输入
    int n = read(0, stdin_buf, sizeof(stdin_buf) - 1); // 留一个字节给 \0
    if (n < 0) {
        fprintf(2, "xargs: read error\n");
        exit(1);
    }
    stdin_buf[n] = '\0'; // 确保缓冲区是一个合法的字符串

    //解析参数，按照\n将参数分割
    char *p = stdin_buf;
    while (*p != '\0') {
        // 跳过所有空白字符（空格、制表符、换行符）
        while (*p == ' ' || *p == '\t' || *p == '\n') {
            *p = '\0';
            p++;
        }
        if (*p == '\0') break;

        // 提取一个参数
        if (stdin_argc < MAXARG - 1) { // 留一个位置给最终的 NULL
            stdin_args[stdin_argc++] = p;
        } else {
            fprintf(2, "xargs: too many arguments\n");
            exit(1);
        }

        // 移动到下一个参数的起始位置
        while (*p != '\0' && *p != ' ' && *p != '\t' && *p != '\n') {
            p++;
        }
    }
    stdin_args[stdin_argc] = 0;
    // 如果没有参数，直接退出
    if (stdin_argc == 0) 
    {
        exit(0);
    }

    //合并出exec命令行参数
    char *exec_argv[MAXARG];
    int exec_argc = 0;
    // 复制目标命令及其参数
    for (int i = 0; cmd_argv[i] != 0; i++) {
        exec_argv[exec_argc++] = cmd_argv[i];
    }

    // 复制从标准输入解析出的参数
    for (int i = 0; stdin_args[i] != 0; i++) {
        if (exec_argc >= MAXARG - 1) {
            fprintf(2, "xargs: too many arguments\n");
            exit(1);
        }
        exec_argv[exec_argc++] = stdin_args[i];
    }

    // 确保 exec_argv 以 NULL 结尾
    exec_argv[exec_argc] = 0;

    // 如果没有任何参数，直接退出
    if (exec_argc == 0) {
        exit(0);
    }
    //创建子进程
    int pid = fork();
    if (pid < 0)
    {
        fprintf(2, "fork error\n");
        exit(1);
    }
    else if (pid == 0)
    {
        exec(exec_argv[0], exec_argv);
    }
    wait(0);
    exit(0);
}
```

