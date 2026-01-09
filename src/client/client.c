#define _POSIX_C_SOURCE 200809L
#include "protocol.h"
#include "network.h"
#include "ui.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

typedef struct {
    int socket;
    GameState current_state;
    bool connected;
    bool game_active;
    int my_player_id;
    pthread_mutex_t state_mutex;
    bool state_updated;
    char connected_host[256];
    int connected_port;
    bool death_handled; // Track if we've already handled the death
} ClientState;

static ClientState client_state;
static volatile bool running = true;
static pid_t server_pid = -1;
static int last_connected_port = DEFAULT_PORT; // Remember last port for rejoin

void signal_handler(int sig) {
    (void)sig;
    running = false;
}

void *receive_thread(void *arg) {
    (void)arg;
    uint8_t buffer[BUFFER_SIZE];
    
    while (running && client_state.connected) {
        ssize_t received = receive_data(client_state.socket, buffer, BUFFER_SIZE);
        if (received <= 0) {
            client_state.connected = false;
            break;
        }
        
        Message msg;
        if (!deserialize_message(buffer, received, &msg)) {
            continue;
        }
        
        switch (msg.type) {
            case MSG_GAME_STATE:
                pthread_mutex_lock(&client_state.state_mutex);
                
                // Free old obstacles if any
                if (client_state.current_state.obstacles) {
                    free(client_state.current_state.obstacles);
                }
                
                client_state.current_state = msg.data.state;
                client_state.state_updated = true;
                
                // Check if game is over
                if (msg.data.state.game_over) {
                    client_state.game_active = false;
                }
                pthread_mutex_unlock(&client_state.state_mutex);
                break;
                
            case MSG_ERROR:
                show_error(msg.data.error_msg);
                client_state.connected = false;
                break;
                
            default:
                break;
        }
    }
    
    return NULL;
}

bool start_local_server(const GameConfig *config, int *port) {
    server_pid = fork();
    
    if (server_pid < 0) {
        return false;
    }
    
    if (server_pid == 0) {
        // Child process - start server
        
        // Redirect stdout and stderr to /dev/null to avoid messing up ncurses UI
        freopen("/dev/null", "w", stdout);
        freopen("/dev/null", "w", stderr);
        
        char port_str[16];
        char width_str[16];
        char height_str[16];
        char time_str[16];
        char max_players_str[16];
        
        snprintf(port_str, sizeof(port_str), "%d", *port);
        snprintf(width_str, sizeof(width_str), "%d", config->width);
        snprintf(height_str, sizeof(height_str), "%d", config->height);
        snprintf(max_players_str, sizeof(max_players_str), "%d", config->max_players);
        
        char *args[20];
        int arg_idx = 0;
        
        args[arg_idx++] = "./server";
        args[arg_idx++] = "-p";
        args[arg_idx++] = port_str;
        args[arg_idx++] = "-w";
        args[arg_idx++] = width_str;
        args[arg_idx++] = "-h";
        args[arg_idx++] = height_str;
        args[arg_idx++] = "-n";
        args[arg_idx++] = max_players_str;
        
        if (config->mode == MODE_TIMED) {
            args[arg_idx++] = "-t";
            snprintf(time_str, sizeof(time_str), "%d", config->time_limit);
            args[arg_idx++] = time_str;
        }
        
        if (config->world_type == WORLD_WITH_OBSTACLES) {
            if (config->load_from_file && config->map_file[0] != '\0') {
                args[arg_idx++] = "-m";
                args[arg_idx++] = (char *)config->map_file;
            } else {
                args[arg_idx++] = "-o";
            }
        }
        
        args[arg_idx] = NULL;
        
        execvp("./server", args);
        exit(1);
    }
    
    // Parent process - wait for server to start
    sleep(2);  // Give server more time to fully initialize
    return true;
}

void stop_local_server(void) {
    if (server_pid > 0) {
        kill(server_pid, SIGTERM);
        waitpid(server_pid, NULL, 0);
        server_pid = -1;
    }
}

bool connect_to_game(const char *host, int port, const char *player_name) {
    client_state.socket = connect_to_server(host, port);
    if (client_state.socket < 0) {
        return false;
    }
    
    // Send join message
    Message msg;
    msg.type = MSG_JOIN_GAME;
    msg.player_id = -1;
    strncpy(msg.data.join_info.name, player_name, MAX_NAME_LENGTH - 1);
    msg.data.join_info.port = port;
    
    uint8_t buffer[BUFFER_SIZE];
    size_t size;
    serialize_message(&msg, buffer, &size);
    
    if (!send_data(client_state.socket, buffer, size)) {
        close_socket(client_state.socket);
        return false;
    }
    
    client_state.connected = true;
    client_state.game_active = true;
    client_state.death_handled = false; // Reset death handled flag
    last_connected_port = port; // Save port for rejoin
    strncpy(client_state.connected_host, host, sizeof(client_state.connected_host) - 1);
    client_state.connected_port = port;
    
    // Start receive thread
    pthread_t thread;
    pthread_create(&thread, NULL, receive_thread, NULL);
    pthread_detach(thread);
    
    // Wait for first state update (longer wait to ensure server initialized snake)
    for (int i = 0; i < 30 && !client_state.state_updated; i++) {
        usleep(100000);
    }
    
    // Extra wait to ensure we get the REAL initialized state (not stale data)
    usleep(200000); // 200ms extra
    
    // Find our player ID
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (strcmp(client_state.current_state.snakes[i].name, player_name) == 0) {
            client_state.my_player_id = i;
            break;
        }
    }
    
    return true;
}

void disconnect_from_game(void) {
    if (client_state.connected) {
        Message msg;
        msg.type = MSG_PLAYER_DISCONNECT;
        msg.player_id = client_state.my_player_id;
        
        uint8_t buffer[BUFFER_SIZE];
        size_t size;
        serialize_message(&msg, buffer, &size);
        send_data(client_state.socket, buffer, size);
        
        client_state.connected = false;
        close_socket(client_state.socket);
    }
}

void game_loop(void) {
    bool paused = false;
    
    while (running && client_state.game_active && client_state.connected) {
        // Handle input
        int ch = getch();
        
        Message msg;
        msg.type = MSG_PLAYER_INPUT;
        msg.player_id = client_state.my_player_id;
        
        bool send_input = false;
        
        switch (ch) {
            case KEY_UP:
            case 'w':
            case 'W':
                msg.data.direction = DIR_UP;
                send_input = true;
                break;
            case KEY_DOWN:
            case 's':
            case 'S':
                msg.data.direction = DIR_DOWN;
                send_input = true;
                break;
            case KEY_LEFT:
            case 'a':
            case 'A':
                msg.data.direction = DIR_LEFT;
                send_input = true;
                break;
            case KEY_RIGHT:
            case 'd':
            case 'D':
                msg.data.direction = DIR_RIGHT;
                send_input = true;
                break;
            case 'p':
            case 'P':
                if (!paused) {
                    msg.type = MSG_PAUSE;
                    paused = true;
                    send_input = true;
                    
                    // Go to menu
                    MenuChoice choice = show_main_menu(true);
                    
                    if (choice == MENU_RESUME_GAME) {
                        msg.type = MSG_RESUME;
                        send_input = true;
                        paused = false;
                    } else if (choice == MENU_EXIT || choice == MENU_NEW_GAME) {
                        disconnect_from_game();
                        return;
                    }
                }
                break;
            case 'q':
            case 'Q':
                disconnect_from_game();
                return;
        }
        
        if (send_input && client_state.connected) {
            uint8_t buffer[BUFFER_SIZE];
            size_t size;
            serialize_message(&msg, buffer, &size);
            send_data(client_state.socket, buffer, size);
        }
        
        // Render
        if (!paused) {
            pthread_mutex_lock(&client_state.state_mutex);
            if (client_state.state_updated) {
                render_game_state(&client_state.current_state, client_state.my_player_id, 
                                 client_state.connected_host, client_state.connected_port);
                
                // Check if our snake died (and we haven't handled it yet)
                if (!client_state.current_state.snakes[client_state.my_player_id].alive && 
                    !client_state.death_handled) {
                    
                    int player_score = client_state.current_state.snakes[client_state.my_player_id].score;
                    int spawn_time = client_state.current_state.snakes[client_state.my_player_id].spawn_time;
                    int survival_time = client_state.current_state.elapsed_time - spawn_time;
                    int player_length = client_state.current_state.snakes[client_state.my_player_id].length;
                    
                    // Skip if this looks like a stale/initial state from server
                    // This happens when server sends state before init_snake completes
                    // or when receiving first state after manual rejoin
                    if (player_score == 0 && survival_time <= 0) {
                        pthread_mutex_unlock(&client_state.state_mutex);
                        // Don't mark as handled - wait for proper state
                    } else {
                        pthread_mutex_unlock(&client_state.state_mutex);
                    
                    render_death_message(player_score, survival_time, 
                                        client_state.connected_host, client_state.connected_port);
                    nodelay(stdscr, FALSE);
                    
                    // Wait for Enter key only
                    int key;
                    do {
                        key = getch();
                    } while (key != '\n' && key != '\r' && key != KEY_ENTER);
                    
                    nodelay(stdscr, TRUE);
                    
                    // Ask if player wants to rejoin
                    clear();
                    int max_y, max_x;
                    getmaxyx(stdscr, max_y, max_x);
                    
                    attron(A_BOLD);
                    mvprintw(max_y / 2 - 1, (max_x - 30) / 2, "Do you want to rejoin?");
                    attroff(A_BOLD);
                    
                    mvprintw(max_y / 2 + 1, (max_x - 20) / 2, "Y = Yes, rejoin game");
                    mvprintw(max_y / 2 + 2, (max_x - 20) / 2, "N = No, back to menu");
                    
                    mvprintw(max_y / 2 + 4, (max_x - 20) / 2, "Your choice: ");
                    refresh();
                    
                    nodelay(stdscr, FALSE);
                    int rejoin;
                    do {
                        rejoin = getch();
                        rejoin = tolower(rejoin);
                    } while (rejoin != 'y' && rejoin != 'n');
                    nodelay(stdscr, TRUE);
                    
                    if (rejoin == 'y') {
                        // Mark death as handled BEFORE reconnecting
                        client_state.death_handled = true;
                        
                        // Save player name before disconnecting
                        char saved_name[MAX_NAME_LENGTH];
                        pthread_mutex_lock(&client_state.state_mutex);
                        strncpy(saved_name, client_state.current_state.snakes[client_state.my_player_id].name, 
                               MAX_NAME_LENGTH - 1);
                        pthread_mutex_unlock(&client_state.state_mutex);
                        
                        // Disconnect first
                        disconnect_from_game();
                        
                        // Show reconnecting message
                        clear();
                        mvprintw(max_y / 2, (max_x - 20) / 2, "Reconnecting...");
                        refresh();
                        usleep(500000); // 0.5s delay
                        
                        // Reconnect (this will reset death_handled to false)
                        if (connect_to_game(client_state.connected_host, last_connected_port, saved_name)) {
                            // Wait for server to send first game state
                            usleep(500000); // 0.5s delay
                            continue;
                        }
                    }
                    
                        // Mark death as handled
                        client_state.death_handled = true;
                        
                        // Player chose not to rejoin, disconnect and return to menu
                        // Give server time to process the disconnect properly
                        usleep(100000); // 100ms delay
                        disconnect_from_game();
                        return;
                    }
                }
                
                if (client_state.current_state.game_over) {
                    show_game_over_stats(&client_state.current_state);
                    pthread_mutex_unlock(&client_state.state_mutex);
                    return;
                }
            }
            pthread_mutex_unlock(&client_state.state_mutex);
        }
        
        usleep(50000); // 50ms
    }
}

int main(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Initialize client state
    memset(&client_state, 0, sizeof(ClientState));
    client_state.socket = -1;
    client_state.my_player_id = -1;
    client_state.death_handled = false;
    pthread_mutex_init(&client_state.state_mutex, NULL);
    
    // Initialize UI
    if (!init_ui()) {
        fprintf(stderr, "Failed to initialize UI\n");
        return 1;
    }
    
    while (running) {
        MenuChoice choice = show_main_menu(false);
        
        switch (choice) {
            case MENU_NEW_GAME: {
                GameConfig config;
                int port = DEFAULT_PORT;
                if (get_game_config(&config, &port)) {
                    
                    // Show starting server message BEFORE starting server
                    clear();
                    int max_y, max_x;
                    getmaxyx(stdscr, max_y, max_x);
                    
                    int box_width = 50;
                    int box_height = 10;
                    int start_x = (max_x - box_width) / 2;
                    int start_y = (max_y - box_height) / 2;
                    
                    // Draw outer box
                    attron(A_BOLD);
                    for (int i = 0; i < box_width; i++) {
                        mvaddch(start_y, start_x + i, '═');
                        mvaddch(start_y + box_height, start_x + i, '═');
                    }
                    for (int i = 1; i < box_height; i++) {
                        mvaddch(start_y + i, start_x, '║');
                        mvaddch(start_y + i, start_x + box_width - 1, '║');
                    }
                    mvaddch(start_y, start_x, '╔');
                    mvaddch(start_y, start_x + box_width - 1, '╗');
                    mvaddch(start_y + box_height, start_x, '╚');
                    mvaddch(start_y + box_height, start_x + box_width - 1, '╝');
                    
                    // Title
                    attron(A_REVERSE);
                    mvprintw(start_y + 2, start_x + (box_width - 20) / 2, "  STARTING SERVER  ");
                    attroff(A_REVERSE);
                    
                    // Server info
                    mvprintw(start_y + 4, start_x + 4, "Mode:       %s", 
                             config.mode == MODE_TIMED ? "Timed" : "Standard");
                    mvprintw(start_y + 5, start_x + 4, "World:      %dx%d", 
                             config.width, config.height);
                    mvprintw(start_y + 6, start_x + 4, "Obstacles:  %s", 
                             config.world_type == WORLD_WITH_OBSTACLES ? "Yes" : "No");
                    mvprintw(start_y + 7, start_x + 4, "Port:       %d", port);
                    
                    // Loading animation
                    mvprintw(start_y + 9, start_x + (box_width - 18) / 2, "Please wait...");
                    
                    attroff(A_BOLD);
                    refresh();
                    
                    if (start_local_server(&config, &port)) {
                        char name[MAX_NAME_LENGTH];
                        
                        clear();
                        nodelay(stdscr, FALSE);
                        echo();
                        
                        int start_y = max_y / 2 - 3;
                        int start_x = max_x / 2 - 20;
                        
                        attron(A_BOLD | A_UNDERLINE);
                        mvprintw(start_y, start_x + 5, "=== ENTER YOUR NAME ===");
                        attroff(A_BOLD | A_UNDERLINE);
                        start_y += 3;
                        
                        attron(A_BOLD);
                        mvprintw(start_y, start_x, "Your name:");
                        attroff(A_BOLD);
                        mvprintw(start_y, start_x + 11, " ");
                        move(start_y, start_x + 12);
                        refresh();
                        
                        getnstr(name, MAX_NAME_LENGTH - 1);
                        noecho();
                        nodelay(stdscr, TRUE);
                        
                        if (strlen(name) > 0) {
                            clear();
                            
                            int box_width = 50;
                            int box_height = 8;
                            int conn_start_x = (max_x - box_width) / 2;
                            int conn_start_y = (max_y - box_height) / 2;
                            
                            // Draw outer box
                            attron(A_BOLD);
                            for (int i = 0; i < box_width; i++) {
                                mvaddch(conn_start_y, conn_start_x + i, '═');
                                mvaddch(conn_start_y + box_height, conn_start_x + i, '═');
                            }
                            for (int i = 1; i < box_height; i++) {
                                mvaddch(conn_start_y + i, conn_start_x, '║');
                                mvaddch(conn_start_y + i, conn_start_x + box_width - 1, '║');
                            }
                            mvaddch(conn_start_y, conn_start_x, '╔');
                            mvaddch(conn_start_y, conn_start_x + box_width - 1, '╗');
                            mvaddch(conn_start_y + box_height, conn_start_x, '╚');
                            mvaddch(conn_start_y + box_height, conn_start_x + box_width - 1, '╝');
                            
                            // Title
                            attron(A_REVERSE);
                            mvprintw(conn_start_y + 2, conn_start_x + (box_width - 16) / 2, "  CONNECTING...  ");
                            attroff(A_REVERSE);
                            
                            // Connection info
                            mvprintw(conn_start_y + 4, conn_start_x + 4, "Player:  %s", name);
                            mvprintw(conn_start_y + 5, conn_start_x + 4, "Server:  127.0.0.1:%d", port);
                            
                            mvprintw(conn_start_y + 7, conn_start_x + (box_width - 18) / 2, "Please wait...");
                            
                            attroff(A_BOLD);
                            refresh();
                            
                            if (connect_to_game("127.0.0.1", port, name)) {
                                game_loop();
                                disconnect_from_game();
                            } else {
                                show_error("Failed to connect to server");
                            }
                        }
                        
                        // Don't stop server here - let it run according to game rules
                        // (10 seconds without players in Standard mode, or until time limit)
                        // stop_local_server();
                    } else {
                        show_error("Failed to start server");
                    }
                }
                break;
            }
            
            case MENU_JOIN_GAME: {
                char host[256];
                int port;
                char name[MAX_NAME_LENGTH];
                
                if (get_connection_info(host, &port, name)) {
                    clear();
                    int max_y, max_x;
                    getmaxyx(stdscr, max_y, max_x);
                    
                    int box_width = 50;
                    int box_height = 8;
                    int conn_start_x = (max_x - box_width) / 2;
                    int conn_start_y = (max_y - box_height) / 2;
                    
                    // Draw outer box
                    attron(A_BOLD);
                    for (int i = 0; i < box_width; i++) {
                        mvaddch(conn_start_y, conn_start_x + i, '═');
                        mvaddch(conn_start_y + box_height, conn_start_x + i, '═');
                    }
                    for (int i = 1; i < box_height; i++) {
                        mvaddch(conn_start_y + i, conn_start_x, '║');
                        mvaddch(conn_start_y + i, conn_start_x + box_width - 1, '║');
                    }
                    mvaddch(conn_start_y, conn_start_x, '╔');
                    mvaddch(conn_start_y, conn_start_x + box_width - 1, '╗');
                    mvaddch(conn_start_y + box_height, conn_start_x, '╚');
                    mvaddch(conn_start_y + box_height, conn_start_x + box_width - 1, '╝');
                    
                    // Title
                    attron(A_REVERSE);
                    mvprintw(conn_start_y + 2, conn_start_x + (box_width - 16) / 2, "  CONNECTING...  ");
                    attroff(A_REVERSE);
                    
                    // Connection info
                    mvprintw(conn_start_y + 4, conn_start_x + 4, "Player:  %s", name);
                    mvprintw(conn_start_y + 5, conn_start_x + 4, "Server:  %s:%d", host, port);
                    
                    mvprintw(conn_start_y + 7, conn_start_x + (box_width - 18) / 2, "Please wait...");
                    
                    attroff(A_BOLD);
                    refresh();
                    
                    if (connect_to_game(host, port, name)) {
                        game_loop();
                        disconnect_from_game();
                    } else {
                        show_error("Failed to connect to server");
                    }
                }
                break;
            }
            
            case MENU_EXIT:
                running = false;
                break;
                
            default:
                break;
        }
    }
    
    cleanup_ui();
    pthread_mutex_destroy(&client_state.state_mutex);
    
    // Stop local server if it was created by this client
    stop_local_server();
    
    return 0;
}
