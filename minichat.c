#include <arpa/inet.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "minichat.h"

int client_sockets[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER; // 初始化互斥锁
int client_count = 0;

// 声明
void *handle_client(void *fd);
void broadcast_message(const char *message, int sender_fd);
void remove_client(int fd);

int main(void) {
  int server_fd, client_fd; // 保存服务端客户端文件描述符
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_len =
      sizeof(client_addr); // accept函数需要知道ip地址大小

  char input_username[] = "你的昵称是：";

  char client_ip[INET_ADDRSTRLEN];
  int client_port;

  // 创建socket
  server_fd = socket(AF_INET, SOCK_STREAM, 0); // AF_INET代表IPv4协议簇
  if (server_fd == -1) {
    perror("socket创建失败");
    exit(EXIT_FAILURE);
  }
  printf("Socket 创建成功\n");

  // 绑定bind ip和port
  // 定义bind()函数所需要的server_addr结构体
  server_addr.sin_family = AF_INET;         // 定义协议簇
  server_addr.sin_addr.s_addr = INADDR_ANY; // 表示监听来自任何ip的信息
  server_addr.sin_port = htons(PORT);       // 小端序转化为大端序

  if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    perror("端口绑定失败");
    close(server_fd);
    exit(EXIT_FAILURE);
  }
  // 绑定的IP和PORT是以socket_addr结构体的形式告知bind函数
  printf("服务端口 %i 绑定成功", PORT);

  // 监听
  if (listen(server_fd, BACKLOG) == -1) {
    perror("监听失败");
    close(server_fd);
    exit(EXIT_FAILURE);
  }
  printf("监听 %i 开启", PORT);

  // 接收客户端信息
  while (1) {
    // accept()为接收信息创建了一个新的fd提供服务
    // accept接收客户端ip信息，并写入client_addr结构体
    client_fd =
        accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
    if (client_fd == -1) {
      perror("accept接收信息失败");
      continue;
    }

    // 获取客户端ip和port(仅仅为了便于服务端调试)
    // 将ip二进制形式转化为可读形式
    inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
    client_port = ntohs(client_addr.sin_port); // 大端序转化为小端序

    // 加锁，处理客户端列表
    pthread_mutex_lock(&clients_mutex);
    if (client_count < MAX_CLIENTS) {
      client_sockets[client_count++] = client_fd;
      printf("新客户端已连接 IP=%s PORT=%i FD=%i", client_ip, client_port,
             client_fd);
    } else {
      printf("客户端数量达到上限，拒绝连接 IP=%s PORT=%i FD=%i", client_ip,
             client_port, client_fd);
      close(client_fd);
      pthread_mutex_unlock(&clients_mutex);
      continue;
    }
    pthread_mutex_unlock(&clients_mutex);

    // 为客户端连接创建线程
    pthread_t thread_id;
    if (pthread_create(&thread_id, NULL, handle_client,
                       (void *)(intptr_t)client_fd) != 0) {
      perror("无法创建线程");
      remove_client(client_fd);
    } else {
      pthread_detach(thread_id); // 回收资源
    }
  }
  close(server_fd);
  return 0;
}

// 具体每个线程处理客户端的内容
void *handle_client(void *fd) {
  // 将传入的无类型指针强制转化为int
  int client_fd = (int)(intptr_t)fd; // intptr_t用于指针的数字安全转换
  char buffer[BUFFERSIZE];
  char username[BUFFERSIZE] = {0};
  char client_ip[INET_ADDRSTRLEN];
  int client_port;
  const char *welcome_mes = "=== 这里是 MiniChat 服务端\n"
                            "=== 这里为公共聊天提供服务\n\n"
                            "请首先输入聊天昵称\n";

  const char *single = ">>>";

  struct sockaddr_in client_addr;
  socklen_t addr_len = sizeof(client_addr);
  // accept是刚开始连接获取ip信息，getpeername是在连接中获取ip信息
  getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len);
  inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
  client_port = ntohs(client_addr.sin_port);

  write(client_fd, welcome_mes, strlen(welcome_mes));

  write(client_fd, "请输入昵称：", 20);

  memset(buffer, 0, BUFFERSIZE);
  int fd_read = read(client_fd, buffer, BUFFERSIZE - 1);
  if (fd_read <= 0) {
    printf("客户端 %s %i 未成功输入用户名，即将断开连接\n", client_ip,
           client_port);
    close(client_fd);
    remove_client(client_fd);
    return NULL;
  }
  buffer[fd_read - 1] = '\0';

  // 对输入的用户名作处理
  char *ptr = strchr(buffer, '\r');
  if (ptr) {
    *ptr = '\0';
  }
  ptr = strchr(buffer, '\n');
  if (ptr) {
    *ptr = '\0';
  }
  strncpy(username, buffer, sizeof(username) - 1);
  printf("User %s 已连接 %s %d fd=%d", username, client_ip, client_port,
         client_fd);

  // 发送欢迎信息
  char welcome[BUFFERSIZE];
  snprintf(welcome, sizeof(welcome), "欢迎 %s 你可以发言了\n\n", username);
  write(client_fd, welcome, strlen(welcome));

  // 广播上线消息
  char online_msg[BUFFERSIZE];
  snprintf(online_msg, sizeof(online_msg), "用户 %s 加入公共聊天室\n\n",
           username);
  broadcast_message(online_msg, client_fd);

  // 广播消息
  while (1) {
    write(client_fd, single, strlen(single));
    memset(buffer, 0, BUFFERSIZE);
    int read_byte = read(client_fd, buffer, BUFFERSIZE - 1);
    if (read_byte <= 0) {
      write(client_fd, "传输失败\n", 5);
      if (read_byte == 0) {
        printf("客户端 %s 断开连接 fd=%d \n", username, client_fd);
        write(client_fd, single, strlen(single));
      } else {
        perror("读取客户端数据失败");
        break;
      }
    }
    buffer[read_byte] = '\0';
    printf("Message: %s : %s \n", username, buffer);

    // 构造广播消息
    char broadcast_buffer[1024];
    // 使用snprintf构建格式化信息
    snprintf(broadcast_buffer, sizeof(broadcast_buffer), "%s >>> %s\n",
             username, buffer);
    broadcast_message(broadcast_buffer, client_fd);
  }

  printf("断开客户端 %s fd=%d\n", username, client_fd);
  // snprintf用于判断用户名能否被读取
  broadcast_message(snprintf(NULL, 0, "用户 %s 离开了聊天室\n", username) > 0
                        ? "某用户离开了聊天室\n"
                        : "有用户离开了聊天室\n",
                    client_fd);
  close(client_fd);
  remove_client(client_fd);
  return NULL;
}

void broadcast_message(const char *message, int sender_fd) {
  pthread_mutex_lock(&clients_mutex);
  for (int i = 0; i < client_count; i++) {
    int fd = client_sockets[i];
    if (fd != sender_fd) {
      if (write(fd, message, strlen(message)) < 0) {
        perror("广播用户消息失败");
      }
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}

void remove_client(int fd) {
  pthread_mutex_lock(&clients_mutex); // 加锁，清除无法创建线程的fd
  for (int i = 0; i < MAX_CLIENTS; i++) {
    if (client_sockets[i] == fd) {
      for (int j = i; i < client_count - 1; j++) {
        client_sockets[j] = client_sockets[j + 1];
      }
      client_count--;
      break;
    }
  }
  pthread_mutex_unlock(&clients_mutex);
}
