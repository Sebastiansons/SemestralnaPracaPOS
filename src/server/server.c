#define _POSIX_C_SOURCE 200809L
#include "protocol.h"
#include "network.h"
#include "game_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

static Game *game = NULL;
static volatile bool server_running = true;

void signal_handler(int sig) {
    (void)sig;
    server_running = false;
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    free(arg);
    
    uint8_t buffer[BUFFER_SIZE];
    int player_id = -1;
    
    while (server_running && game->running) {
        ssize_t received = receive_data(client_socket, buffer, BUFFER_SIZE);
        if (received <= 0) {
            break;
        }
        
        Message msg;
        if (!deserialize_message(buffer, received, &msg)) {
            continue;
        }
        
        switch (msg.type) {
            case MSG_JOIN_GAME:
                if (player_id == -1) {
                    player_id = add_player(game, client_socket, msg.data.join_info.name);
                    if (player_id == -1) {
                        Message error_msg;
                        error_msg.type = MSG_ERROR;
                        if (game->state.max_players == 1) {
                            strcpy(error_msg.data.error_msg, "Game is singleplayer only");
                        } else {
                            snprintf(error_msg.data.error_msg, sizeof(error_msg.data.error_msg),
                                    "Game is full (%d/%d players)", 
                                    game->state.player_count, game->state.max_players);
                        }
                        size_t size;
                        serialize_message(&error_msg, buffer, &size);
                        send_data(client_socket, buffer, size);
                        goto cleanup;
                    }
                    printf("Player %d (%s) joined the game\n", player_id, msg.data.join_info.name);
                }
                break;
                
            case MSG_PLAYER_INPUT:
                if (player_id != -1) {
                    handle_player_input(game, player_id, msg.data.direction);
                }
                break;
                
            case MSG_PAUSE:
                if (player_id != -1) {
                    pause_player(game, player_id);
                }
                break;
                
            case MSG_RESUME:
                if (player_id != -1) {
                    resume_player(game, player_id);
                }
                break;
                
            case MSG_PLAYER_DISCONNECT:
                goto cleanup;
                
            default:
                break;
        }
    }
    
cleanup:
    if (player_id != -1) {
        printf("Player %d disconnected\n", player_id);
        remove_player(game, player_id);
    }
    close_socket(client_socket);
    return NULL;
}

void *game_loop(void *arg) {
    (void)arg;
    
    while (server_running && game->running && !game->state.game_over) {
        update_game(game);
        broadcast_game_state(game);
        usleep(1000000 / TICK_RATE); // 100ms for 10 ticks/sec
    }
    
    // Game over - send final state
    if (game->state.game_over) {
        printf("Game over!\n");
        broadcast_game_state(game);
        
        // Wait a bit for clients to receive final state
        sleep(2);
    }
    
    server_running = false;
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = DEFAULT_PORT;
    GameConfig config;
    
    // Default configuration
    config.mode = MODE_STANDARD;
    config.world_type = WORLD_NO_OBSTACLES;
    config.width = 40;
    config.height = 20;
    config.time_limit = 0;
    config.load_from_file = false;
    config.map_file[0] = '\0';
    config.max_players = MAX_PLAYERS; // Default to multiplayer
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {
            config.width = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {
            config.height = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {
            config.mode = MODE_TIMED;
            config.time_limit = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-o") == 0) {
            config.world_type = WORLD_WITH_OBSTACLES;
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            config.world_type = WORLD_WITH_OBSTACLES;
            config.load_from_file = true;
            strncpy(config.map_file, argv[i + 1], sizeof(config.map_file) - 1);
            i++;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            config.max_players = atoi(argv[i + 1]);
            if (config.max_players < 1) config.max_players = 1;
            if (config.max_players > MAX_PLAYERS) config.max_players = MAX_PLAYERS;
            i++;
        }
    }
    
    srand(time(NULL));
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("Starting Snake Game Server...\n");
    printf("Port: %d\n", port);
    printf("Mode: %s\n", config.mode == MODE_STANDARD ? "Standard" : "Timed");
    printf("World: %dx%d\n", config.width, config.height);
    printf("Obstacles: %s\n", config.world_type == WORLD_WITH_OBSTACLES ? "Yes" : "No");
    printf("Max players: %d (%s)\n", config.max_players, 
           config.max_players == 1 ? "Singleplayer" : "Multiplayer");
    
    // Create game
    game = create_game(&config);
    if (!game) {
        fprintf(stderr, "Failed to create game\n");
        return 1;
    }
    
    // Create server socket
    int server_socket = create_server_socket(port);
    if (server_socket < 0) {
        fprintf(stderr, "Failed to create server socket\n");
        destroy_game(game);
        return 1;
    }
    
    printf("Server listening on port %d\n", port);
    
    // Start game loop thread
    pthread_t game_thread;
    pthread_create(&game_thread, NULL, game_loop, NULL);
    
    // Accept clients
    while (server_running && !game->state.game_over) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server_socket, &readfds);
        
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int activity = select(server_socket + 1, &readfds, NULL, NULL, &timeout);
        
        if (activity > 0 && FD_ISSET(server_socket, &readfds)) {
            int client_socket = accept_client(server_socket);
            if (client_socket >= 0) {
                printf("New client connected\n");
                
                int *sock_ptr = malloc(sizeof(int));
                *sock_ptr = client_socket;
                
                pthread_t thread;
                pthread_create(&thread, NULL, client_handler, sock_ptr);
                pthread_detach(thread);
            }
        }
    }
    
    // Cleanup
    printf("Shutting down server...\n");
    pthread_join(game_thread, NULL);
    close_socket(server_socket);
    destroy_game(game);
    
    printf("Server stopped\n");
    return 0;
}
