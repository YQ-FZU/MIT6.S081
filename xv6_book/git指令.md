git指令：

```c
git add .	//暂存所有未提交的修改
git commit -m "说明"		//提交到当前分支

git stash list	//列出当前存储
git stash save "你的描述信息，例如：修复登录bug的中间状态"	//暂存当前分支
git stash apply stash@{1}	//应用存储
git stash drop stash@{1}	//删除暂时存储
__attribute__((noreturn))
```

gdb调试xv6

1、在一个终端里启动qemu

```c
make qemu-gdb CPUS=1		//启动qemu，并开启gdb调试模式，启动单核调试
```

2、再另外一个终端里远程连接qemu

```c
gdb-multiarch                //启动支持RISC-V的GDB（普通gdb不行）
file kernel/kernel  		// 加载xv6内核符号表，关联函数名/变量名和内存地址
target remote localhost:26000  //连接到QEMU的26000端口（和终端1的端口一致）
```



## git提交代码

```c
# 检查Git是否已配置身份（提交代码必须的基础配置）
git config --list | grep user
```

如果没有配置身份则配置

```c
# 配置用户名（替换为你的 GitHub 用户名，比如 YQ-FZU）
git config --global user.name "你的GitHub用户名"

# 配置邮箱（替换为你的 GitHub 绑定邮箱，比如 xxx@xxx.com）
git config --global user.email "你的GitHub绑定邮箱"
```



```c
# 检查本地Git仓库状态（确认无未处理的错误）
git status
```

```c
git branch		//查看当前所在分支
```

Git 的提交流程是「工作区 → 暂存区 → 本地仓库 → 远程仓库」

1、暂存所有修改的文件

```c
# 暂存所有修改的文件（. 代表当前目录下所有文件）
git add .
# 验证暂存结果（可选，确认文件已被暂存）
git status
```

2、本地提交暂存的修改

```c
# 提交暂存区的文件，-m后写清晰的提交信息
git commit -m "xv6 lab3：完成分支三的xxx实验修改（如页表/内存管理）"
```

3、查看关联远程仓库

```c
# 添加/修改 origin 指向你的仓库（替换为你的仓库地址）
git remote add origin https://github.com/YQ-FZU/MIT6.S081.git
# 若提示 origin 已存在，用以下命令覆盖：
git remote set-url origin https://github.com/YQ-FZU/MIT6.S081.git
git remote -v
```

4、推送分支到git仓库

```c
# 推送branch3分支到origin远程，并关联本地/远程分支（-u是关键）
git push -u origin branch3
```

-u 参数的全称是 --set-upstream，作用是「关联本地 branch3 分支和远程 origin/branch3 分支」；
第一次推送新分支必须加 -u，后续再修改该分支推送时，只需执行 git push 即可（无需重复写分支名）；
如果分支名有空格（比如「分支三」），必须用引号括起来：git push -u origin "分支三"，但强烈建议用无空格的 branch3，避免命令解析错误。





## 常用git命令

1、git fetch upstream

```c
// 从 upstream（MIT 官方仓库）获取代码（这是你复用官方仓库的核心命令）
git fetch upstream
```

git fetch 只会「下载远程分支的更新到本地」（比如把 origin/syscall、upstream/main 的最新代码下载到本地 .git 目录），不会修改你当前正在工作的本地分支（比如你在 syscall 分支写的代码不会被覆盖），这也是它比 git pull 更安全的原因（git pull = git fetch + git merge，会直接合并代码，容易出冲突）。

注意：git fetch默认是获取远程origin的代码跟新到本地

2、git checkout 分支名

git checkout 默认切换的是本地分支；如果本地没有该分支，但远程仓库（如 origin）有对应的分支，Git 会自动创建本地分支并关联远程分支。

（1）、本地分支存在

```c
git branch			//查看分支
git checkout syscall	//切换到syscall分支
```

此时切换的是你本地的 syscall 分支（包含你自己修改的 xv6 代码），和远程仓库无关，只是切换本地工作环境。

（2）、本地分支lab4不存在

```c
git checkout lab4
```

Git 会自动做两件事：在本地创建 lab4 分支；将本地 lab4 分支关联到远程 origin/lab4 分支；等价于手动执行：git checkout -b lab4 origin/lab4

（3）、查看远程分支

```c
# 先从 upstream 获取最新代码
git fetch upstream
# 切换到 upstream/main（远程分支），注意：这是「分离头指针」状态
git checkout upstream/main
```

注意：这种状态下修改代码无法提交（因为不是本地分支），仅用于查看官方代码，看完后要切回自己的本地分支（比如 git checkout syscall）。



3、git push将本地修改提交远程仓库

```c
git push [选项] [远程仓库名] [本地分支名]:[远程分支名]
```

（1）、首次推送分支

```c
git push -u origin 本地分支名
```

-u：关联本地分支和远程分支，第一次推送时使用，告诉git本地分支应该关联那一个远程分支，第二次可以直接使用git push

（2）、推送指定分支（本地 / 远程分支名不同时）

```c
git push origin lab3:xv6-lab3
```

你本地分支名是 lab3，想推送到远程并命名为 xv6-lab3（方便区分）。



4、git merge：把远程更新合并到本地分支

```c
# 下载 origin 远程（你的 GitHub 仓库）所有分支的最新代码
git fetch origin
    
# 切换到本地 syscall 分支（你要同步最新代码的分支）
git checkout syscall
    
# 把 origin/syscall（远程最新）合并到本地 syscall 分支
git merge origin/syscall
```

