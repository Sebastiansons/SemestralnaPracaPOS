#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

int create_server_socket(int port) {
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        return -1;
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

int accept_client(int server_socket) {
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int client_socket;
    
    client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (client_socket < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("accept");
        }
        return -1;
    }
    
    return client_socket;
}

int connect_to_server(const char *host, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation error");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    // Convert address
    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
        perror("invalid address");
        close(sock);
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connection failed");
        close(sock);
        return -1;
    }
    
    return sock;
}

bool send_data(int socket, const uint8_t *data, size_t size) {
    // First send the size
    uint32_t net_size = htonl((uint32_t)size);
    if (send(socket, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {
        return false;
    }
    
    // Then send the data
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(socket, data + sent, size - sent, 0);
        if (n <= 0) {
            return false;
        }
        sent += n;
    }
    
    return true;
}

ssize_t receive_data(int socket, uint8_t *buffer, size_t buffer_size) {
    // First receive the size
    uint32_t net_size;
    ssize_t n = recv(socket, &net_size, sizeof(net_size), 0);
    if (n != sizeof(net_size)) {
        return -1;
    }
    
    size_t size = ntohl(net_size);
    if (size > buffer_size) {
        return -1;
    }
    
    // Then receive the data
    size_t received = 0;
    while (received < size) {
        n = recv(socket, buffer + received, size - received, 0);
        if (n <= 0) {
            return -1;
        }
        received += n;
    }
    
    return (ssize_t)size;
}

void close_socket(int socket) {
    if (socket >= 0) {
        close(socket);
    }
}
