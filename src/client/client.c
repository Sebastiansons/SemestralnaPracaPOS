#include <unistd.h>
#include "protocol.h"
#include "network.h"
#include "ui.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <ctype.h>

typedef struct {//stav klienta
    int socket;//socket spojenia
    GameState current_state;//aktualny stav hry
    bool connected;//ci je pripojeny
    bool game_active;//ci je hra aktivna
    int my_player_id;//ID mojho hraca
    pthread_mutex_t state_mutex;//mutex pre thread-safe pristup
    bool state_updated;//ci bol stav aktualizovany
    char connected_host[256];//host servera
    int connected_port;//port servera
    bool death_handled;//ci bola smrt uz spracovana
} ClientState;

static ClientState client_state;//globalny stav klienta
static volatile bool running = true;//klient bezi
static pid_t server_pid = -1;//PID lokalneho servera
static int last_connected_port = DEFAULT_PORT;//posledny pouzity port

void signal_handler(int sig) {//spracovanie signalov (Ctrl+C)
    (void)sig;
    running = false;//zastav klienta
}

void *receive_thread(void *arg) {//vlakno pre prijem sprav od servera
    (void)arg;
    uint8_t buffer[BUFFER_SIZE];//buffer pre prijem dat
    
    while (running && client_state.connected) {//kym bezi klient a je pripojeny
        ssize_t received = receive_data(client_state.socket, buffer, BUFFER_SIZE);//prijmi data
        if (received <= 0) {//chyba alebo odpojenie
            client_state.connected = false;
            break;
        }
        
        Message msg;
        if (!deserialize_message(buffer, received, &msg)) {//deserializuj spravu
            continue;//neplatna sprava
        }
        
        switch (msg.type) {//spracuj spravu podla typu
            case MSG_GAME_STATE://stav hry od servera
                pthread_mutex_lock(&client_state.state_mutex);//zamkni mutex
                
                // Free old obstacles if any
                if (client_state.current_state.obstacles) {//uvolni stare prekazky
                    free(client_state.current_state.obstacles);
                }
                
                client_state.current_state = msg.data.state;//uloz novy stav
                client_state.state_updated = true;//oznac stav ako aktualizovany
                
                // Check if game is over
                if (msg.data.state.game_over) {//ak je hra ukoncena
                    client_state.game_active = false;
                }
                pthread_mutex_unlock(&client_state.state_mutex);//odomkni mutex
                break;
                
            case MSG_ERROR://chybova sprava od servera
                show_error(msg.data.error_msg);//zobraz chybu
                client_state.connected = false;//odpoj sa
                break;
                
            default:
                break;
        }
    }
    
    return NULL;
}

bool start_local_server(const GameConfig *config, int *port) {//spusti lokalny server ako child proces
    server_pid = fork();//vytvor child proces
    
    if (server_pid < 0) {//chyba pri forku
        return false;
    }
    
    if (server_pid == 0) {//sme v child procese
        // Child process - start server
        
        // Redirect stdout and stderr to /dev/null to avoid messing up ncurses UI
        freopen("/dev/null", "w", stdout);//presmeruj stdout do /dev/null (aby neskreslil ncurses)
        freopen("/dev/null", "w", stderr);//presmeruj stderr do /dev/null
        
        char port_str[16];//stringy pre argumenty servera
        char width_str[16];
        char height_str[16];
        char time_str[16];
        char max_players_str[16];
        
        snprintf(port_str, sizeof(port_str), "%d", *port);//konvertuj cisla na stringy
        snprintf(width_str, sizeof(width_str), "%d", config->width);
        snprintf(height_str, sizeof(height_str), "%d", config->height);
        snprintf(max_players_str, sizeof(max_players_str), "%d", config->max_players);
        
        char *args[20];//pole argumentov pre execvp
        int arg_idx = 0;
        
        args[arg_idx++] = "./server";//nazov programu
        args[arg_idx++] = "-p";//port
        args[arg_idx++] = port_str;
        args[arg_idx++] = "-w";//sirka
        args[arg_idx++] = width_str;
        args[arg_idx++] = "-h";//vyska
        args[arg_idx++] = height_str;
        args[arg_idx++] = "-n";//max pocet hracov
        args[arg_idx++] = max_players_str;
        
        if (config->mode == MODE_TIMED) {//ak je casovany mod
            args[arg_idx++] = "-t";
            snprintf(time_str, sizeof(time_str), "%d", config->time_limit);
            args[arg_idx++] = time_str;
        }
        
        if (config->world_type == WORLD_WITH_OBSTACLES) {//ak ma prekazky
            if (config->load_from_file && config->map_file[0] != '\0') {//nacitaj zo suboru
                args[arg_idx++] = "-m";
                args[arg_idx++] = (char *)config->map_file;
            } else {//generuj nahodne
                args[arg_idx++] = "-o";
            }
        }
        
        args[arg_idx] = NULL;//ukoncovaci NULL
        
        execvp("./server", args);//spusti server (nahradi tento proces)
        exit(1);//ak execvp zlyhalo
    }
    
    // Parent process - wait for server to start
    // Check if server started successfully by trying to connect
    sleep(1);//pocakaj 1 sekundu aby server nabehol
    
    // Try to verify server is listening (with retries)
    for (int i = 0; i < 3; i++) {//skus 3x ci server nabehol
        int test_sock = connect_to_server("127.0.0.1", *port);//pokus sa pripojit
        if (test_sock >= 0) {//ak sa podarilo pripojit
            close_socket(test_sock);//zatvor testovaci socket
            return true;//server bezi!
        }
        usleep(500000);//cakaj 0.5s medzi pokusmi
    }
    
    // Server didn't start - kill the child process
    kill(server_pid, SIGTERM);//zastavme server ak nenabehol
    waitpid(server_pid, NULL, 0);//pocakaj na ukoncenie child procesu
    server_pid = -1;//vynuluj PID
    return false;//server sa nespustil
}

void stop_local_server(void) {//zastavi lokalny server
    if (server_pid > 0) {//ak je server spusteny
        kill(server_pid, SIGTERM);//posli signal SIGTERM
        waitpid(server_pid, NULL, 0);//pocakaj na ukoncenie
        server_pid = -1;//vynuluj PID
    }
}

bool connect_to_game(const char *host, int port, const char *player_name) {//pripoj sa k serveru a vstup do hry
    client_state.socket = connect_to_server(host, port);//pripoj sa k serveru
    if (client_state.socket < 0) {//ak sa nepodarilo pripojit
        return false;
    }
    
    // Send join message
    Message msg;//vytvor join spravu
    msg.type = MSG_JOIN_GAME;//typ - pripojenie do hry
    msg.player_id = -1;//este nemame ID
    strncpy(msg.data.join_info.name, player_name, MAX_NAME_LENGTH - 1);//meno hraca
    msg.data.join_info.port = port;//port servera
    
    uint8_t buffer[BUFFER_SIZE];//buffer pre serializaciu
    size_t size;
    serialize_message(&msg, buffer, &size);//serializuj spravu
    
    if (!send_data(client_state.socket, buffer, size)) {//posli join spravu
        close_socket(client_state.socket);
        return false;
    }
    
    client_state.connected = true;//sme pripojeni
    client_state.game_active = true;//hra je aktivna
    client_state.death_handled = false;//resetuj flag smrti
    last_connected_port = port;//uloz port pre rejoin
    strncpy(client_state.connected_host, host, sizeof(client_state.connected_host) - 1);//uloz host
    client_state.connected_port = port;//uloz port
    
    // Start receive thread
    pthread_t thread;//vlakno pre prijem sprav
    pthread_create(&thread, NULL, receive_thread, NULL);//spusti vlakno
    pthread_detach(thread);//odpoj vlakno (automaticke upratanie)
    
    // Wait for first state update (longer wait to ensure server initialized snake)
    for (int i = 0; i < 30 && !client_state.state_updated; i++) {//cakaj na prvy stav (max 3s)
        usleep(100000);//100ms
    }
    
    // Extra wait to ensure we get the REAL initialized state (not stale data)
    usleep(200000);//este 200ms pre istotu (aby sme dostali spravne inicializovany stav)
    
    // Find our player ID
    for (int i = 0; i < MAX_PLAYERS; i++) {//najdi nase ID podla mena
        if (strcmp(client_state.current_state.snakes[i].name, player_name) == 0) {
            client_state.my_player_id = i;//uloz nase ID
            break;
        }
    }
    
    return true;//pripojenie uspesne
}

void disconnect_from_game(void) {//odpoj sa od hry
    if (client_state.connected) {//ak sme pripojeni
        Message msg;//vytvor disconnect spravu
        msg.type = MSG_PLAYER_DISCONNECT;//typ - odpojenie
        msg.player_id = client_state.my_player_id;//nase ID
        
        uint8_t buffer[BUFFER_SIZE];//buffer pre serializaciu
        size_t size;
        serialize_message(&msg, buffer, &size);//serializuj spravu
        send_data(client_state.socket, buffer, size);//posli serveru
        
        client_state.connected = false;//uz nie sme pripojeni
        close_socket(client_state.socket);//zatvor socket
    }
}

void game_loop(void) {//hlavna hernia slucka klienta
    bool locally_paused = false;//ci sme lokalne v pause menu
    
    while (running && client_state.game_active && client_state.connected) {//kym bezi klient, hra je aktivna a sme pripojeni
        // Handle input
        int ch = getch();//precitaj klavesovu spravu (non-blocking)
        
        Message msg;//sprava pre server
        msg.type = MSG_PLAYER_INPUT;//typ - vstup hraca
        msg.player_id = client_state.my_player_id;//nase ID
        
        bool send_input = false;//ci poslat spravu
        
        switch (ch) {//spracuj klavesy
            case KEY_UP://sipka hore
            case 'w':
            case 'W':
                msg.data.direction = DIR_UP;//smer hore
                send_input = true;
                break;
            case KEY_DOWN://sipka dole
            case 's':
            case 'S':
                msg.data.direction = DIR_DOWN;//smer dole
                send_input = true;
                break;
            case KEY_LEFT://sipka vlavo
            case 'a':
            case 'A':
                msg.data.direction = DIR_LEFT;//smer vlavo
                send_input = true;
                break;
            case KEY_RIGHT://sipka vpravo
            case 'd':
            case 'D':
                msg.data.direction = DIR_RIGHT;//smer vpravo
                send_input = true;
                break;
            case 'p'://pauza
            case 'P':
                if (!locally_paused) {//ak este nie sme v pauze
                    // Send MSG_PAUSE immediately
                    msg.type = MSG_PAUSE;//posli pause spravu
                    if (client_state.connected) {
                        uint8_t buffer[BUFFER_SIZE];
                        size_t size;
                        serialize_message(&msg, buffer, &size);
                        send_data(client_state.socket, buffer, size);
                    }
                    
                    locally_paused = true;//sme v pauze
                    
                    // Go to menu (this blocks until user makes a choice)
                    MenuChoice choice = show_main_menu(true);//zobraz menu (blokuje)
                    
                    if (choice == MENU_RESUME_GAME) {//hrac chce pokracovat
                        // Send MSG_RESUME immediately
                        msg.type = MSG_RESUME;//posli resume spravu
                        if (client_state.connected) {
                            uint8_t buffer[BUFFER_SIZE];
                            size_t size;
                            serialize_message(&msg, buffer, &size);
                            send_data(client_state.socket, buffer, size);
                        }
                        locally_paused = false;//uz nie sme v pauze
                    } else if (choice == MENU_EXIT || choice == MENU_NEW_GAME) {//hrac chce odist
                        disconnect_from_game();//odpoj sa
                        return;
                    }
                }
                break;
            case 'q'://quit
            case 'Q':
                disconnect_from_game();//odpoj sa
                return;
        }
        
        if (send_input && client_state.connected) {//ak mame poslat vstup a sme pripojeni
            uint8_t buffer[BUFFER_SIZE];
            size_t size;
            serialize_message(&msg, buffer, &size);//serializuj spravu
            send_data(client_state.socket, buffer, size);//posli serveru
        }
        
        // Render - always render to show game state, even when in pause menu
        // This allows seeing other players and countdown
        pthread_mutex_lock(&client_state.state_mutex);//zamkni mutex
        if (client_state.state_updated) {//ak bol stav aktualizovany
                render_game_state(&client_state.current_state, client_state.my_player_id, 
                                 client_state.connected_host, client_state.connected_port);//vykresli stav hry
                
                // Check if our snake died (and we haven't handled it yet)
                if (!client_state.current_state.snakes[client_state.my_player_id].alive && 
                    !client_state.death_handled) {//ak nas had zomrel a este sme to nespracovali
                    
                    int player_score = client_state.current_state.snakes[client_state.my_player_id].score;//skore
                    int spawn_time = client_state.current_state.snakes[client_state.my_player_id].spawn_time;//cas spawnu
                    int survival_time = client_state.current_state.elapsed_time - spawn_time;//cas prezitia
                    
                    // Skip if this looks like a stale/initial state from server
                    // This happens when server sends state before init_snake completes
                    // or when receiving first state after manual rejoin
                    if (player_score == 0 && survival_time <= 0) {//ak vyzerá ako stary/inicialny stav
                        pthread_mutex_unlock(&client_state.state_mutex);
                        // Don't mark as handled - wait for proper state
                    } else {//normalny stav smrti
                        pthread_mutex_unlock(&client_state.state_mutex);
                    
                    render_death_message(player_score, survival_time, 
                                        client_state.connected_host, client_state.connected_port);//zobraz death screen
                    nodelay(stdscr, FALSE);//prepni na blocking mod
                    
                    // Wait for Enter key only
                    int key;
                    do {//cakaj na Enter
                        key = getch();
                    } while (key != '\n' && key != '\r' && key != KEY_ENTER);
                    
                    nodelay(stdscr, TRUE);//prepni spat na non-blocking
                    
                    // Ask if player wants to rejoin
                    clear();//vycisti obrazovku
                    int max_y, max_x;
                    getmaxyx(stdscr, max_y, max_x);//zisti rozmery
                    
                    attron(A_BOLD);
                    mvprintw(max_y / 2 - 1, (max_x - 30) / 2, "Do you want to rejoin?");//opytaj sa ci chce rejoinnut
                    attroff(A_BOLD);
                    
                    mvprintw(max_y / 2 + 1, (max_x - 20) / 2, "Y = Yes, rejoin game");
                    mvprintw(max_y / 2 + 2, (max_x - 20) / 2, "N = No, back to menu");
                    
                    mvprintw(max_y / 2 + 4, (max_x - 20) / 2, "Your choice: ");
                    refresh();
                    
                    nodelay(stdscr, FALSE);//prepni na blocking
                    int rejoin;
                    do {//cakaj na Y alebo N
                        rejoin = getch();
                        rejoin = tolower(rejoin);
                    } while (rejoin != 'y' && rejoin != 'n');
                    nodelay(stdscr, TRUE);//prepni spat na non-blocking
                    
                    if (rejoin == 'y') {//hrac chce rejoinnut
                        // Mark death as handled BEFORE reconnecting
                        client_state.death_handled = true;//oznac smrt ako spracovanu
                        
                        // Save player name before disconnecting
                        char saved_name[MAX_NAME_LENGTH];//uloz meno pred odpojenym
                        pthread_mutex_lock(&client_state.state_mutex);
                        strncpy(saved_name, client_state.current_state.snakes[client_state.my_player_id].name, 
                               MAX_NAME_LENGTH - 1);
                        pthread_mutex_unlock(&client_state.state_mutex);
                        
                        // Disconnect first
                        disconnect_from_game();//najprv sa odpoj
                        
                        // Show reconnecting message
                        clear();
                        mvprintw(max_y / 2, (max_x - 20) / 2, "Reconnecting...");//zobraz spravu
                        refresh();
                        usleep(500000);//cakaj 0.5s
                        
                        // Reconnect (this will reset death_handled to false)
                        if (connect_to_game(client_state.connected_host, last_connected_port, saved_name)) {//pripoj sa znova
                            // Wait for server to send first game state
                            usleep(500000);//cakaj 0.5s na prvy stav
                            continue;//pokracuj v slucke
                        }
                    } else {//hrac nechce rejoinnut
                        // Mark death as handled
                        client_state.death_handled = true;//oznac smrt ako spracovanu
                        
                        // Player chose not to rejoin, disconnect and return to menu
                        // Give server time to process the disconnect properly
                        usleep(100000);//cakaj 100ms
                        disconnect_from_game();//odpoj sa
                        return;//vrat sa do menu
                    }
                }
                
                if (client_state.current_state.game_over) {//ak je hra ukoncena
                    show_game_over_stats(&client_state.current_state);//zobraz statistiky
                    pthread_mutex_unlock(&client_state.state_mutex);
                    return;
                }
            }
            pthread_mutex_unlock(&client_state.state_mutex);//odomkni mutex
        }
        
        usleep(50000);//cakaj 50ms (20 fps)
    }
}

int main(void) {//hlavna funkcia klienta
    signal(SIGINT, signal_handler);//nastav handler pre Ctrl+C
    signal(SIGTERM, signal_handler);//nastav handler pre SIGTERM
    
    // Initialize client state
    memset(&client_state, 0, sizeof(ClientState));//vynuluj stav klienta
    client_state.socket = -1;//ziadny socket
    client_state.my_player_id = -1;//ziadne ID
    client_state.death_handled = false;//smrt nebola spracovana
    pthread_mutex_init(&client_state.state_mutex, NULL);//inicializuj mutex
    
    // Initialize UI
    if (!init_ui()) {//inicializuj ncurses UI
        fprintf(stderr, "Failed to initialize UI\n");
        return 1;
    }
    
    while (running) {//hlavna slucka klienta
        MenuChoice choice = show_main_menu(false);//zobraz hlavne menu
        
        switch (choice) {//spracuj volbu z menu
            case MENU_NEW_GAME: {//nova hra (spusti lokalny server)
                GameConfig config;//konfiguracia hry
                int port = DEFAULT_PORT;//default port
                if (get_game_config(&config, &port)) {//ziskaj konfiguraciu od uzivatela
                    
                    // Show starting server message BEFORE starting server
                    clear();//vycisti obrazovku
                    int max_y, max_x;
                    getmaxyx(stdscr, max_y, max_x);//zisti rozmery terminalu
                    
                    int box_width = 50;
                    int box_height = 10;
                    int start_x = (max_x - box_width) / 2;
                    int start_y = (max_y - box_height) / 2;
                    
                    // Draw outer box
                    attron(A_BOLD);
                    for (int i = 0; i < box_width; i++) {
                        mvaddch(start_y, start_x + i, ACS_HLINE);
                        mvaddch(start_y + box_height, start_x + i, ACS_HLINE);
                    }
                    for (int i = 1; i < box_height; i++) {
                        mvaddch(start_y + i, start_x, ACS_VLINE);
                        mvaddch(start_y + i, start_x + box_width - 1, ACS_VLINE);
                    }
                    mvaddch(start_y, start_x, ACS_ULCORNER);
                    mvaddch(start_y, start_x + box_width - 1, ACS_URCORNER);
                    mvaddch(start_y + box_height, start_x, ACS_LLCORNER);
                    mvaddch(start_y + box_height, start_x + box_width - 1, ACS_LRCORNER);
                    
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
                    
                    if (start_local_server(&config, &port)) {//ak sa podarilo spustit server
                        char name[MAX_NAME_LENGTH];//meno hraca
                        
                        clear();//vycisti obrazovku
                        nodelay(stdscr, FALSE);//prepni na blocking vstup
                        echo();//zobrazuj zadavane znaky
                        
                        int start_y = max_y / 2 - 3;//zaciatok v strede
                        int start_x = max_x / 2 - 20;
                        
                        attron(A_BOLD | A_UNDERLINE);
                        mvprintw(start_y, start_x + 5, "=== ENTER YOUR NAME ===");//nadpis
                        attroff(A_BOLD | A_UNDERLINE);
                        start_y += 3;
                        
                        attron(A_BOLD);
                        mvprintw(start_y, start_x, "Your name:");//prompt
                        attroff(A_BOLD);
                        mvprintw(start_y, start_x + 11, " ");
                        move(start_y, start_x + 12);//kurzor na poziciu
                        refresh();
                        
                        getnstr(name, MAX_NAME_LENGTH - 1);//precitaj meno
                        flushinp();//vycisti input buffer
                        noecho();//vypni echo
                        nodelay(stdscr, TRUE);//prepni spat na non-blocking
                        
                        if (strlen(name) > 0) {//ak bolo zadane meno
                            clear();
                            
                            int box_width = 50;
                            int box_height = 8;
                            int conn_start_x = (max_x - box_width) / 2;
                            int conn_start_y = (max_y - box_height) / 2;
                            
                            // Draw outer box
                            attron(A_BOLD);
                            for (int i = 0; i < box_width; i++) {
                                mvaddch(conn_start_y, conn_start_x + i, ACS_HLINE);
                                mvaddch(conn_start_y + box_height, conn_start_x + i, ACS_HLINE);
                            }
                            for (int i = 1; i < box_height; i++) {
                                mvaddch(conn_start_y + i, conn_start_x, ACS_VLINE);
                                mvaddch(conn_start_y + i, conn_start_x + box_width - 1, ACS_VLINE);
                            }
                            mvaddch(conn_start_y, conn_start_x, ACS_ULCORNER);
                            mvaddch(conn_start_y, conn_start_x + box_width - 1, ACS_URCORNER);
                            mvaddch(conn_start_y + box_height, conn_start_x, ACS_LLCORNER);
                            mvaddch(conn_start_y + box_height, conn_start_x + box_width - 1, ACS_LRCORNER);
                            
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
                            
                            if (connect_to_game("127.0.0.1", port, name)) {//pripoj sa k serveru
                                game_loop();//spusti hernu slucku
                                disconnect_from_game();//odpoj sa po skonceni
                            } else {
                                show_error("Failed to connect to server");//zobraz chybu
                            }
                        }
                        
                        // Don't stop server here - let it run according to game rules
                        // (10 seconds without players in Standard mode, or until time limit)
                        // stop_local_server(); - nezastavuj server, nech bezi podla pravidiel
                    } else {
                        show_error("Failed to start server. Port may be in use.");//chyba pri spusteni servera
                    }
                }
                break;
            }
            
            case MENU_JOIN_GAME: {//pripojenie k existujucemu serveru
                int port;//port servera
                char name[MAX_NAME_LENGTH];//meno hraca
                
                if (get_connection_info(&port, name)) {//ziskaj info od uzivatela
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
                        mvaddch(conn_start_y, conn_start_x + i, ACS_HLINE);
                        mvaddch(conn_start_y + box_height, conn_start_x + i, ACS_HLINE);
                    }
                    for (int i = 1; i < box_height; i++) {
                        mvaddch(conn_start_y + i, conn_start_x, ACS_VLINE);
                        mvaddch(conn_start_y + i, conn_start_x + box_width - 1, ACS_VLINE);
                    }
                    mvaddch(conn_start_y, conn_start_x, ACS_ULCORNER);
                    mvaddch(conn_start_y, conn_start_x + box_width - 1, ACS_URCORNER);
                    mvaddch(conn_start_y + box_height, conn_start_x, ACS_LLCORNER);
                    mvaddch(conn_start_y + box_height, conn_start_x + box_width - 1, ACS_LRCORNER);
                    
                    // Title
                    attron(A_REVERSE);
                    mvprintw(conn_start_y + 2, conn_start_x + (box_width - 16) / 2, "  CONNECTING...  ");
                    attroff(A_REVERSE);
                    
                    // Connection info - always localhost
                    mvprintw(conn_start_y + 4, conn_start_x + 4, "Player:  %s", name);
                    mvprintw(conn_start_y + 5, conn_start_x + 4, "Server:  localhost:%d", port);
                    
                    mvprintw(conn_start_y + 7, conn_start_x + (box_width - 18) / 2, "Please wait...");
                    
                    attroff(A_BOLD);
                    refresh();
                    
                    if (connect_to_game("127.0.0.1", port, name)) {//pripoj sa k serveru (vzdy localhost)
                        game_loop();//spusti hernu slucku
                        disconnect_from_game();//odpoj sa po skonceni
                    } else {
                        show_error("Failed to connect to server");//zobraz chybu
                    }
                }
                break;
            }
            
            case MENU_EXIT://ukoncenie aplikacie
                running = false;//zastav hlavnu slucku
                break;
                
            default:
                break;
        }
    }
    
    cleanup_ui();//uprac ncurses UI
    pthread_mutex_destroy(&client_state.state_mutex);//znic mutex
    
    // Stop local server if it was created by this client
    stop_local_server();//zastav lokalny server ak bol spusteny
    
    return 0;
}
