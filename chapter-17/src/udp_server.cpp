// udp_server.cpp - UDP 服务器示例
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    // 1. 创建 UDP Socket
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }
    
    // 2. 绑定地址和端口
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8888);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(sockfd);
        return 1;
    }
    
    printf("UDP Server listening on port 8888...\n");
    
    // 3. 接收和响应
    char buffer[1024];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    while (1) {
        // 接收数据
        ssize_t n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0,
                             (struct sockaddr*)&client_addr, &addr_len);
        if (n < 0) {
            perror("recvfrom");
            continue;
        }
        
        buffer[n] = '\0';
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Received from %s:%d: %s\n", 
               client_ip, ntohs(client_addr.sin_port), buffer);
        
        // 发送响应
        const char* response = "Message received!";
        sendto(sockfd, response, strlen(response), 0,
               (struct sockaddr*)&client_addr, addr_len);
    }
    
    close(sockfd);
    return 0;
}
