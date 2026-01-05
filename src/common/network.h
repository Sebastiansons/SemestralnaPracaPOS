#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

#define DEFAULT_PORT 8888
#define BUFFER_SIZE 65536

// Network functions
int create_server_socket(int port);
int accept_client(int server_socket);
int connect_to_server(const char *host, int port);
bool send_data(int socket, const uint8_t *data, size_t size);
ssize_t receive_data(int socket, uint8_t *buffer, size_t buffer_size);
void close_socket(int socket);

#endif // NETWORK_H
