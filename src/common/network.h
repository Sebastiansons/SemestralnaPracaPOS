/**
 * @file network.h
 * @brief TCP socket networking functions
 * 
 * Provides abstraction layer for TCP socket operations used in client-server
 * communication. Handles server socket creation, client connections,
 * data transmission, and socket cleanup.
 */

#ifndef NETWORK_H
#define NETWORK_H

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <sys/types.h>

/** Default server port */
#define DEFAULT_PORT 8888

/** Maximum buffer size for network operations */
#define BUFFER_SIZE 65536

/**
 * @brief Create and bind server socket
 * @param port Port number to bind to (1024-65535 recommended)
 * @return Socket file descriptor on success, -1 on failure
 * 
 * Creates TCP socket, sets SO_REUSEADDR, binds to specified port,
 * and starts listening with backlog of 10.
 */
int create_server_socket(int port);

/**
 * @brief Accept incoming client connection
 * @param server_socket Server socket file descriptor
 * @return Client socket file descriptor on success, -1 on failure
 * 
 * Blocks until client connects. Returns new socket for client communication.
 */
int accept_client(int server_socket);

/**
 * @brief Connect to server as client
 * @param host Server hostname or IP address (e.g., "127.0.0.1")
 * @param port Server port number
 * @return Socket file descriptor on success, -1 on failure
 * 
 * Creates TCP socket and connects to specified server.
 */
int connect_to_server(const char *host, int port);

/**
 * @brief Send data over socket
 * @param socket Socket file descriptor
 * @param data Data buffer to send
 * @param size Size of data in bytes
 * @return true if all data sent successfully, false on error
 * 
 * Ensures all data is sent (handles partial sends).
 */
bool send_data(int socket, const uint8_t *data, size_t size);

/**
 * @brief Receive data from socket
 * @param socket Socket file descriptor
 * @param buffer Buffer to store received data
 * @param buffer_size Maximum buffer size
 * @return Number of bytes received, 0 on connection close, -1 on error
 * 
 * Blocking receive operation. Returns actual number of bytes received.
 */
ssize_t receive_data(int socket, uint8_t *buffer, size_t buffer_size);

/**
 * @brief Close socket connection
 * @param socket Socket file descriptor to close
 * 
 * Safely closes socket and releases resources.
 */
void close_socket(int socket);

#endif // NETWORK_H
