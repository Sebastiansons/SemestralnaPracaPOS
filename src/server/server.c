#include <unistd.h>
#include "protocol.h"
#include "network.h"
#include "game_logic.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <sys/select.h>
#include <sys/time.h>

static Game *game = NULL;//globalna hra
static volatile bool server_running = true;//server bezi

void signal_handler(int sig) {//spracovanie signalov (Ctrl+C)
    (void)sig;
    server_running = false;//zastav server
}

void *client_handler(void *arg) {//vlakno pre obsluhu jedneho klienta
    int client_socket = *(int *)arg;//socket klienta
    free(arg);//uvolni argument
    
    uint8_t buffer[BUFFER_SIZE];//buffer pre prijem dat
    int player_id = -1;//ID hraca (este nepridelene)
    
    while (server_running && game->running) {//kym bezi server a hra
        ssize_t received = receive_data(client_socket, buffer, BUFFER_SIZE);//prijmi data od klienta
        if (received <= 0) {//chyba alebo odpojenie
            break;
        }
        
        Message msg;
        if (!deserialize_message(buffer, received, &msg)) {//deserializuj spravu
            continue;//neplatna sprava, ignoruj
        }
        
        switch (msg.type) {//spracuj spravu podla typu
            case MSG_JOIN_GAME://klient sa chce pripojit do hry
                if (player_id == -1) {//ak este nema pridelene ID
                    player_id = add_player(game, client_socket, msg.data.join_info.name);//pridaj hraca
                    if (player_id == -1) {//ak sa nepodarilo pridat (hra plna)
                        Message error_msg;//vytvor chybovu spravu
                        error_msg.type = MSG_ERROR;
                        if (game->state.max_players == 1) {
                            strcpy(error_msg.data.error_msg, "Game is singleplayer only");
                        } else {
                            snprintf(error_msg.data.error_msg, sizeof(error_msg.data.error_msg),
                                    "Game is full (%d/%d players)", 
                                    game->state.player_count, game->state.max_players);
                        }
                        size_t size;
                        serialize_message(&error_msg, buffer, &size);//serializuj chybu
                        send_data(client_socket, buffer, size);//posli chybu klientovi
                        goto cleanup;//ukonci spojenie
                    }
                    printf("Player %d (%s) joined the game\n", player_id, msg.data.join_info.name);
                }
                break;
                
            case MSG_PLAYER_INPUT://vstup od hraca (smer)
                if (player_id != -1) {
                    handle_player_input(game, player_id, msg.data.direction);//spracuj vstup
                }
                break;
                
            case MSG_PAUSE://pozastavenie hry
                if (player_id != -1) {
                    pause_player(game, player_id);//pozastav hraca
                }
                break;
                
            case MSG_RESUME://obnovenie hry
                if (player_id != -1) {
                    resume_player(game, player_id);//obnov hraca
                }
                break;
                
            case MSG_PLAYER_DISCONNECT://hrac sa odpaja
                goto cleanup;//ukonci spojenie
                
            default:
                break;
        }
    }
    
cleanup://upratanie po odpojeni klienta
    if (player_id != -1) {//ak mal pridelene ID
        printf("Player %d disconnected\n", player_id);
        remove_player(game, player_id);//odstran hraca z hry
    }
    close_socket(client_socket);//zatvor socket
    return NULL;
}

void *game_loop(void *arg) {//hlavna slucka hry (bezi v samostatnom vlakne)
    (void)arg;
    
    while (server_running && game->running && !game->state.game_over) {//kym bezi server, hra a nie je game over
        update_game(game);//aktualizuj stav hry (jeden tick)
        broadcast_game_state(game);//posli stav vsetkym klientom
        usleep(1000000 / TICK_RATE);//cakaj 100ms (10 tikov za sekundu)
    }
    
    // Game over - send final state
    if (game->state.game_over) {//ak je hra ukoncena
        printf("Game over!\n");
        broadcast_game_state(game);//posli finalne stav
        
        // Wait a bit for clients to receive final state
        sleep(2);//pocakaj aby klienti dostali finalny stav
        
        // Stop the server
        server_running = false;//zastav server
    }
    
    server_running = false;//zastav server
    return NULL;
}

int main(int argc, char *argv[]) {//hlavna funkcia servera
    int port = DEFAULT_PORT;//default port 8888
    GameConfig config;//konfiguracia hry
    
    // Default configuration
    config.mode = MODE_STANDARD;//standardny mod (10s po odpojeni posledneho hraca)
    config.world_type = WORLD_NO_OBSTACLES;//bez prekazok (zabalovanie mapy)
    config.width = 40;//sirka 40
    config.height = 20;//vyska 20
    config.time_limit = 0;//bez casoveho limitu
    config.load_from_file = false;//negeneruj z mapy
    config.map_file[0] = '\0';//prazdny nazov suboru
    config.max_players = MAX_PLAYERS;//max 8 hracov (multiplayer)
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {//spracuj argumenty prikazoveho riadka
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {//port
            port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-w") == 0 && i + 1 < argc) {//sirka
            config.width = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-h") == 0 && i + 1 < argc) {//vyska
            config.height = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "-t") == 0 && i + 1 < argc) {//casovany mod
            config.mode = MODE_TIMED;
            config.time_limit = atoi(argv[i + 1]);//casovy limit v sekundach
            i++;
        } else if (strcmp(argv[i], "-o") == 0) {//s prekazkami
            config.world_type = WORLD_WITH_OBSTACLES;
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {//nacitaj mapu zo suboru
            config.world_type = WORLD_WITH_OBSTACLES;
            config.load_from_file = true;
            strncpy(config.map_file, argv[i + 1], sizeof(config.map_file) - 1);
            i++;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {//max pocet hracov
            config.max_players = atoi(argv[i + 1]);
            if (config.max_players < 1) config.max_players = 1;//min 1
            if (config.max_players > MAX_PLAYERS) config.max_players = MAX_PLAYERS;//max 8
            i++;
        }
    }
    
    srand(time(NULL));//inicializuj generator nahodnych cisel
    signal(SIGINT, signal_handler);//nastav handler pre Ctrl+C
    signal(SIGTERM, signal_handler);//nastav handler pre SIGTERM
    
    printf("Starting Snake Game Server...\n");
    printf("Port: %d\n", port);
    printf("Mode: %s\n", config.mode == MODE_STANDARD ? "Standard" : "Timed");
    printf("World: %dx%d\n", config.width, config.height);
    printf("Obstacles: %s\n", config.world_type == WORLD_WITH_OBSTACLES ? "Yes" : "No");
    printf("Max players: %d (%s)\n", config.max_players, 
           config.max_players == 1 ? "Singleplayer" : "Multiplayer");
    
    // Create game
    game = create_game(&config);//vytvor hru s danou konfiguraciou
    if (!game) {//ak sa nepodarilo vytvorit
        fprintf(stderr, "Failed to create game\n");
        return 1;
    }
    
    // Create server socket
    int server_socket = create_server_socket(port);//vytvor serverovy socket
    if (server_socket < 0) {//ak sa nepodarilo vytvorit
        fprintf(stderr, "Failed to create server socket\n");
        destroy_game(game);
        return 1;
    }
    
    printf("Server listening on port %d\n", port);
    
    // Start game loop thread
    pthread_t game_thread;//vlakno pre hernu slucku
    pthread_create(&game_thread, NULL, game_loop, NULL);//spusti hernu slucku v samostatnom vlakne
    
    // Accept clients
    while (server_running && !game->state.game_over) {//kym bezi server a hra nie je ukoncena
        fd_set readfds;//mnozina file descriptorov
        FD_ZERO(&readfds);//vymaz mnozinu
        FD_SET(server_socket, &readfds);//pridaj serverovy socket
        
        struct timeval timeout;//timeout pre select
        timeout.tv_sec = 1;//1 sekunda
        timeout.tv_usec = 0;
        
        int activity = select(server_socket + 1, &readfds, NULL, NULL, &timeout);//cakaj na aktivitu alebo timeout
        
        if (activity > 0 && FD_ISSET(server_socket, &readfds)) {//ak je aktivita na serverovom sockete
            int client_socket = accept_client(server_socket);//prijmi klienta
            if (client_socket >= 0) {//ak sa podarilo prijat
                printf("New client connected\n");
                
                int *sock_ptr = malloc(sizeof(int));//alokuj pamat pre socket descriptor
                *sock_ptr = client_socket;
                
                pthread_t thread;//vlakno pre klienta
                pthread_create(&thread, NULL, client_handler, sock_ptr);//spusti handler v samostatnom vlakne
                pthread_detach(thread);//odpoj vlakno (automaticke upratanie po skonceni)
            }
        }
    }
    
    // Cleanup
    printf("Shutting down server...\n");
    pthread_join(game_thread, NULL);//pocakaj na ukoncenie hernej slucky
    close_socket(server_socket);//zatvor serverovy socket
    destroy_game(game);//znic hru a uvolni zdroje
    
    printf("Server stopped\n");
    return 0;
}
