#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> //kniznice pre pracu so socketmi, sietovymi adresami a chybami.
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

int create_server_socket(int port) {//vytvori serverovy socket na danom porte.
    int server_fd;//file descriptor socketu, adresova struktura a volba pre socket
    struct sockaddr_in address;
    int opt = 1;
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) { //TCP socket (SOCK_STREAM) pre IPv4 (AF_INET). Ak zlyha, vrati 0.
        perror("socket failed");
        return -1;
    }
    
    // Set socket options - moznost SO_REUSEADDR, aby sa port dal okamzite znovu pouzit po zatvoreni.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        return -1;
    }
    
    address.sin_family = AF_INET;//nastavi rodinu adries na IPv4.
    address.sin_addr.s_addr = INADDR_ANY;//server, aby pocuval na vsetkych dostupnych sietovych rozhraniach.
    address.sin_port = htons(port);//port a prevedie ho do sietoveho byte order (big-endian).
    
    // Bind socket -  pripoji (bind) socket k zadanej adrese a portu.
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        return -1;
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {//nastavi socket do rezimu pocuvania s frontou max. 10 cakajucich pripojeni.
        perror("listen");
        close(server_fd);
        return -1;
    }
    
    return server_fd;
}

int accept_client(int server_socket) {//prijme nove klientske pripojenie na serverovom sockete.
    struct sockaddr_in address;//struktura pre adresu klienta
    int addrlen = sizeof(address);
    int client_socket;
    
    client_socket = accept(server_socket, (struct sockaddr *)&address, (socklen_t*)&addrlen);//blokuje, kym sa nepripoji klient
    if (client_socket < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {//ak chyba nie je "by zablokoval", vypis chybu
            perror("accept");
        }
        return -1;
    }
    
    return client_socket;//vrati file descriptor klientskeho socketu.
}

int connect_to_server(const char *host, int port) {//klientska funkcia pre pripojenie k serveru.
    int sock = 0;//socket descriptor
    struct sockaddr_in serv_addr;//struktura adresy servera
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {//vytvori TCP socket
        perror("socket creation error");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;//IPv4
    serv_addr.sin_port = htons(port);//port servera v sietovom byte order
    
    // Convert address
    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {//konvertuje IP adresu zo stringu do binarnej formy
        perror("invalid address");
        close(sock);
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {//pripoji sa k serveru
        perror("connection failed");
        close(sock);
        return -1;
    }
    
    return sock;//vrati socket pripojeny k serveru
}

bool send_data(int socket, const uint8_t *data, size_t size) {//posle data cez socket (najprv velkost, potom data)
    // First send the size
    uint32_t net_size = htonl((uint32_t)size);//velkost dat v sietovom byte order
    if (send(socket, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {//posli velkost
        return false;
    }
    
    // Then send the data
    size_t sent = 0;//pocet uz poslanych bajtov
    while (sent < size) {//posielaj, kym nepojdu vsetky data
        ssize_t n = send(socket, data + sent, size - sent, 0);//posli zvysok dat
        if (n <= 0) {//chyba alebo socket zatvoreny
            return false;
        }
        sent += n;//pripocitaj pocet poslanych bajtov
    }
    
    return true;//vsetko uspesne odoslane
}

ssize_t receive_data(int socket, uint8_t *buffer, size_t buffer_size) {//prijme data zo socketu (najprv velkost, potom data)
    // First receive the size
    uint32_t net_size;
    ssize_t n = recv(socket, &net_size, sizeof(net_size), 0);//prijmi velkost dat
    if (n != sizeof(net_size)) {//chyba pri prijimani velkosti
        return -1;
    }
    
    size_t size = ntohl(net_size);//konvertuj velkost z network byte order
    if (size > buffer_size) {//data su vacsie ako buffer
        return -1;
    }
    
    // Then receive the data
    size_t received = 0;//pocet uz prijatych bajtov
    while (received < size) {//prijimaj, kym neprijdes vsetky data
        n = recv(socket, buffer + received, size - received, 0);//prijmi zvysok dat
        if (n <= 0) {//chyba alebo socket zatvoreny
            return -1;
        }
        received += n;//pripocitaj pocet prijatych bajtov
    }
    
    return (ssize_t)size;//vrat celkovy pocet prijatych bajtov
}

void close_socket(int socket) {//bezpecne zatvori socket
    if (socket >= 0) {//ak je socket platny
        close(socket);//zatvor ho
    }
}
