# MiniChat
这是一个简单的基于tcp协议的聊天服务端
加入多线程功能
你可以使用git来查看整个程序的开发过程

## 注意
这个程序只能在支持POSIX标准的操作系统中编译运行(Linux/Unix)


# Usage
使用netcat连接
eg: nc IP PORT 或者 telnet IP PORT


# 计划
计划在后来创建基于tcp的自有协议，编写一个客户端，重构服务端
增加账号，密码功能


# 概述
## 多线程 
采用POSIX标准的库（pthread.h），利用互斥锁处理每次修改内存的行为
利用数组存放多个用户的fd,在编译时需要添加-pthread参数
## 
