// simple_tcp_client.cpp - 最简单的 TCP 客户端示例
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    // 1. 创建 Socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }
    
    // 2. 设置服务器地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);
    
    // 3. 连接服务器
    printf("Connecting to server...\n");
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return 1;
    }
    
    printf("Connected!\n");
    
    // 4. 发送数据
    const char* msg = "Hello, Server!";
    printf("Sending: %s\n", msg);
    send(sockfd, msg, strlen(msg), 0);
    
    // 5. 接收响应
    char buffer[1024];
    int n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        printf("Received: %s\n", buffer);
    }
    
    // 6. 关闭连接
    close(sockfd);
    printf("Connection closed\n");
    
    return 0;
}
