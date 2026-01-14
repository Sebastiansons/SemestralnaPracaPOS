#include "game_logic.h"
#include "map.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

Game *create_game(const GameConfig *config) {//vytvori novu hru s danou konfiguaciou
    Game *game = (Game *)malloc(sizeof(Game));//alokuj pamat pre hru
    if (!game) {
        return NULL;
    }
    
    memset(game, 0, sizeof(Game));//vynuluj celu strukturu
    
    // Initialize game state
    game->state.game_id = rand();//nahodne ID hry
    game->state.player_count = 0;//zatial ziadni hraci
    game->state.width = config->width;//sirka mapy
    game->state.height = config->height;//vyska mapy
    game->state.mode = config->mode;//mod hry (standard/timed)
    game->state.time_limit = config->time_limit;//casovy limit
    game->state.elapsed_time = 0;//zatial neuplynul cas
    game->state.game_over = false;//hra nie je ukoncena
    game->state.food_count = 0;//zatial ziadne jedlo
    game->state.max_players = config->max_players;//max pocet hracov
    
    // Load or generate map
    if (config->load_from_file && config->map_file[0] != '\0') {//ak sa ma nacitat mapa zo suboru
        int w, h;
        if (load_map_from_file(config->map_file, &game->state.obstacles, &w, &h)) {//pokus sa nacitat
            game->state.width = w;//pouzij rozmery zo suboru
            game->state.height = h;
        } else {
            // Failed to load map, generate random obstacles instead
            Position center = {config->width / 2, config->height / 2};//stred mapy
            int max_attempts = 10;//max pokusov
            bool success = false;
            
            for (int attempt = 0; attempt < max_attempts; attempt++) {//opakuj kym sa nevygeneruje platna mapa
                generate_random_map(&game->state.obstacles, config->width, config->height, 0.10f);//generuj s 10% hustotou
                
                // Verify reachability
                if (is_reachable(game->state.obstacles, config->width, config->height, center)) {//over ci su vsetky bunky dosiahnutelne
                    success = true;
                    break;
                }
                
                // Free and try again
                free_obstacles(game->state.obstacles);//uvolni a skus znova
                game->state.obstacles = NULL;
            }
            
            // If all attempts failed, create empty map
            if (!success) {//ak vsetky pokusy zlyhali
                game->state.obstacles = (uint8_t *)calloc(config->width * config->height, sizeof(uint8_t));//vytvor prazdnu mapu
            }
        }
    } else if (config->world_type == WORLD_WITH_OBSTACLES) {//ak ma byt svet s prekazkami
        // Generate random map with obstacles - retry until reachable
        Position center = {config->width / 2, config->height / 2};//stred mapy
        int max_attempts = 10;//max pokusov
        bool success = false;
        
        for (int attempt = 0; attempt < max_attempts; attempt++) {//opakuj kym sa nevygeneruje platna mapa
            generate_random_map(&game->state.obstacles, config->width, config->height, 0.10f);//generuj s 10% hustotou
            
            // Verify reachability
            if (is_reachable(game->state.obstacles, config->width, config->height, center)) {//over ci su vsetky bunky dosiahnutelne
                success = true;
                break;
            }
            
            // Free and try again
            free_obstacles(game->state.obstacles);//uvolni a skus znova
            game->state.obstacles = NULL;
        }
        
        // If all attempts failed, create empty map
        if (!success) {//ak vsetky pokusy zlyhali
            game->state.obstacles = (uint8_t *)calloc(config->width * config->height, sizeof(uint8_t));//vytvor prazdnu mapu
        }
    } else {
        // No obstacles
        game->state.obstacles = (uint8_t *)calloc(config->width * config->height, sizeof(uint8_t));//prazdna mapa bez prekazok
    }
    
    pthread_mutex_init(&game->mutex, NULL);//inicializuj mutex pre thread-safe pristup
    game->running = true;//hra bezi
    game->start_time = time(NULL);//cas spustenia hry
    game->last_player_time = time(NULL);//cas posledneho pripojeneho hraca
    game->config = *config;//uloz konfiguraciu
    
    for (int i = 0; i < MAX_PLAYERS; i++) {//inicializuj vsetkych hracov
        game->client_sockets[i] = -1;//ziadny socket
        game->client_connected[i] = false;//nepripojeny
        game->pause_countdown[i] = 0;//ziadny countdown
    }
    
    return game;//vrat vytvorenu hru
}

void destroy_game(Game *game) {//znici hru a uvolni vsetky zdroje
    if (!game) {//ak hra neexistuje
        return;
    }
    
    game->running = false;//zastav hru
    
    pthread_mutex_lock(&game->mutex);//zamkni mutex
    
    if (game->state.obstacles) {//ak existuju prekazky
        free_obstacles(game->state.obstacles);//uvolni pamat
    }
    
    pthread_mutex_unlock(&game->mutex);//odomkni mutex
    pthread_mutex_destroy(&game->mutex);//znic mutex
    
    free(game);//uvolni pamat hry
}

int add_player(Game *game, int socket, const char *name) {//prida hraca do hry
    pthread_mutex_lock(&game->mutex);//zamkni mutex
    
    // Check if game is full based on max_players setting
    if (game->state.player_count >= game->state.max_players) {//ak je hra plna
        pthread_mutex_unlock(&game->mutex);
        return -1;//chyba
    }
    
    // Find free player slot
    int player_id = -1;//najdi volny slot
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!game->client_connected[i]) {//ak slot nie je obsadeny
            player_id = i;
            break;
        }
    }
    
    if (player_id == -1) {//ak nie je volny slot
        pthread_mutex_unlock(&game->mutex);
        return -1;//chyba
    }
    
    // Find spawn position
    int start_x = game->state.width / 2 + (player_id % 4) * 5;//vypocitaj spawn poziciu
    int start_y = game->state.height / 2 + (player_id / 4) * 5;//okolo stredu mapy
    
    // Make sure spawn position is valid
    while (start_x >= game->state.width - 3) start_x -= 5;//over ci je v hraniciach
    while (start_y >= game->state.height - 3) start_y -= 5;
    
    // Clear the snake slot completely (in case it was used before)
    memset(&game->state.snakes[player_id], 0, sizeof(Snake));//vycisti slot hada
    
    init_snake(&game->state.snakes[player_id], player_id, start_x, start_y, name);//inicializuj hada
    game->state.snakes[player_id].spawn_time = game->state.elapsed_time;//nastav cas spawnu
    game->client_sockets[player_id] = socket;//uloz socket
    game->client_connected[player_id] = true;//oznac ako pripojeny
    game->pause_countdown[player_id] = 30;//3 sekundy countdown (10 ticks/sec)
    game->state.player_count++;//zvys pocet hracov
    game->last_player_time = time(NULL);//aktualizuj cas posledneho hraca
    
    // Generate food for new player
    generate_food(game);//vygeneruj jedlo
    
    pthread_mutex_unlock(&game->mutex);//odomkni mutex
    
    return player_id;//vrat ID hraca
}

void remove_player(Game *game, int player_id) {//odstrani hraca z hry
    pthread_mutex_lock(&game->mutex);//zamkni mutex
    
    if (player_id >= 0 && player_id < MAX_PLAYERS && game->client_connected[player_id]) {//ak je hraci ID platne
        game->state.snakes[player_id].alive = false;//had umrie
        game->client_connected[player_id] = false;//odpoj hraca
        game->state.player_count--;//zniz pocet hracov
        game->last_player_time = time(NULL);//aktualizuj cas posledneho hraca
    }
    
    pthread_mutex_unlock(&game->mutex);//odomkni mutex
}

void generate_food(Game *game) {//generuje jedlo na mape
    // Generate food equal to number of active players
    int target_food = 0;//cielovy pocet jedla
    for (int i = 0; i < MAX_PLAYERS; i++) {//spocitaj aktivnych hracov
        if (game->client_connected[i] && game->state.snakes[i].alive) {
            target_food++;//jedno jedlo na ziveho hraca
        }
    }
    
    while (game->state.food_count < target_food) {//kym nemame dost jedla
        Position food_pos;//pozicia jedla
        bool valid = false;
        int attempts = 0;//pocet pokusov
        
        while (!valid && attempts < 100) {//skus najst platnu poziciu (max 100 pokusov)
            food_pos.x = rand() % game->state.width;//nahodna x pozicia
            food_pos.y = rand() % game->state.height;//nahodna y pozicia
            
            // Check if position is valid
            valid = is_valid_position(game, food_pos);//over ci je pozicia platna (nie prekazka, nie had)
            
            // Check if food already exists here
            for (int i = 0; i < game->state.food_count && valid; i++) {//over ci tam uz nie je jedlo
                if (game->state.food[i].x == food_pos.x && game->state.food[i].y == food_pos.y) {
                    valid = false;
                }
            }
            
            attempts++;
        }
        
        if (valid) {//ak sa nasla platna pozicia
            game->state.food[game->state.food_count++] = food_pos;//pridaj jedlo
        } else {
            break;//inak skonci
        }
    }
}

bool is_valid_position(Game *game, Position pos) {//skontroluje ci je pozicia platna (nie prekazka, nie had, v hraniciach)
    // Check bounds
    if (pos.x < 0 || pos.x >= game->state.width || pos.y < 0 || pos.y >= game->state.height) {//mimo hranice
        return false;
    }
    
    // Check obstacles
    if (game->state.obstacles && is_obstacle(game->state.obstacles, pos.x, pos.y, game->state.width)) {//je prekazka
        return false;
    }
    
    // Check snakes
    for (int i = 0; i < MAX_PLAYERS; i++) {//over vsetkych hadov
        if (game->client_connected[i] && is_position_on_snake(&game->state.snakes[i], pos)) {//pozicia je na hadovi
            return false;
        }
    }
    
    return true;//pozicia je platna
}

void update_game(Game *game) {//aktualizuje stav hry (jeden tick)
    pthread_mutex_lock(&game->mutex);//zamkni mutex
    
    // Update elapsed time
    game->state.elapsed_time = (int)(time(NULL) - game->start_time);//aktualizuj uplynuly cas
    
    // Check timed mode
    if (game->state.mode == MODE_TIMED && game->state.time_limit > 0) {//ak je casovany mod
        if (game->state.elapsed_time >= game->state.time_limit) {//cas vyprsal
            game->state.game_over = true;//hra konci
            pthread_mutex_unlock(&game->mutex);
            return;
        }
    }
    
    // Check standard mode (10 seconds without players)
    if (game->state.mode == MODE_STANDARD && game->state.player_count == 0) {//standardny mod bez hracov
        if (time(NULL) - game->last_player_time >= 10) {//10 sekund bez hracov
            game->state.game_over = true;//hra konci
            pthread_mutex_unlock(&game->mutex);
            return;
        }
    }
    
    // Update pause countdowns (only for resume countdown)
    for (int i = 0; i < MAX_PLAYERS; i++) {//aktualizuj pause countdown pre kazdeho hraca
        if (game->pause_countdown[i] > 0) {//ak bezi countdown
            game->pause_countdown[i]--;//zniz o 1
            if (game->pause_countdown[i] == 0) {//ak sa countdown skoncil
                game->state.snakes[i].paused = false;//zrus pauzu
            }
        }
    }
    
    // Move snakes
    bool wrap_around = (game->state.obstacles == NULL ||//over mapu (mode bez prekazok)
                        is_obstacle(game->state.obstacles, 0, 0, game->state.width) == false);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {//pohni vsetkymi hadmi
        if (game->client_connected[i] && game->state.snakes[i].alive && !game->state.snakes[i].paused) {//ak je zivy a nie pozastaveny
            move_snake(&game->state.snakes[i], game->state.width, game->state.height, wrap_around);//pohni hadom
        }
    }
    
    // Check collisions
    for (int i = 0; i < MAX_PLAYERS; i++) {//skontroluj kolizie pre kazdeho hada
        if (!game->client_connected[i] || !game->state.snakes[i].alive) {//preskoc odpojenych a mrtvych
            continue;
        }
        
        Position head = game->state.snakes[i].positions[0];//pozicia hlavy hada
        
        // Check wall collision (if not wrapping)
        if (!wrap_around) {
            if (head.x < 0 || head.x >= game->state.width ||//had vysel mimo hranice
                head.y < 0 || head.y >= game->state.height) {
                game->state.snakes[i].alive = false;//had umiera
                continue;
            }
        }
        
        // Check obstacle collision
        if (game->state.obstacles && is_obstacle(game->state.obstacles, head.x, head.y, game->state.width)) {//narazil do prekazky
            game->state.snakes[i].alive = false;//had umiera
            continue;
        }
        
        // Check self collision
        if (check_self_collision(&game->state.snakes[i])) {//narazil sam do seba
            game->state.snakes[i].alive = false;//had umiera
            continue;
        }
        
        // Check collision with other snakes
        for (int j = 0; j < MAX_PLAYERS; j++) {//over kolizie s inymi hadmi
            if (i != j && game->client_connected[j]) {
                if (check_collision_with_snake(&game->state.snakes[i], &game->state.snakes[j])) {//narazil do ineho hada
                    game->state.snakes[i].alive = false;//had umiera
                    break;
                }
            }
        }
        
        // Check food collision
        for (int f = 0; f < game->state.food_count; f++) {//over kolizie s jedlom
            if (head.x == game->state.food[f].x && head.y == game->state.food[f].y) {//zjedol jedlo
                grow_snake(&game->state.snakes[i]);//zvacsi hada
                // Remove this food and shift array
                for (int k = f; k < game->state.food_count - 1; k++) {//odstran jedlo z pola
                    game->state.food[k] = game->state.food[k + 1];
                }
                game->state.food_count--;//zniz pocet jedla
                break;
            }
        }
    }
    
    // Generate food if needed
    generate_food(game);//vygeneruj nove jedlo ak treba
    
    pthread_mutex_unlock(&game->mutex);//odomkni mutex
}

void broadcast_game_state(Game *game) {//posle aktualny stav hry vsetkym pripojenym klientom
    pthread_mutex_lock(&game->mutex);//zamkni mutex
    
    Message msg;//vytvor spravu
    msg.type = MSG_GAME_STATE;//typ spravy - stav hry
    msg.player_id = -1;//ziadny konkretny hrac
    msg.data.state = game->state;//skopiruj stav hry
    
    uint8_t buffer[BUFFER_SIZE];//buffer pre serializaciu
    size_t size;
    serialize_message(&msg, buffer, &size);//serializuj spravu
    
    for (int i = 0; i < MAX_PLAYERS; i++) {//posli vsetkym pripojenym klientom
        if (game->client_connected[i] && game->client_sockets[i] >= 0) {
            send_data(game->client_sockets[i], buffer, size);//posli data
        }
    }
    
    pthread_mutex_unlock(&game->mutex);//odomkni mutex
}

void handle_player_input(Game *game, int player_id, Direction direction) {//spracuje vstup od hraca (zmena smeru)
    pthread_mutex_lock(&game->mutex);//zamkni mutex
    
    if (player_id >= 0 && player_id < MAX_PLAYERS && game->client_connected[player_id]) {//ak je hrac platny
        change_direction(&game->state.snakes[player_id], direction);//zmen smer hada
    }
    
    pthread_mutex_unlock(&game->mutex);//odomkni mutex
}

void pause_player(Game *game, int player_id) {//pozastavi hada hraca
    pthread_mutex_lock(&game->mutex);//zamkni mutex
    
    if (player_id >= 0 && player_id < MAX_PLAYERS && game->client_connected[player_id]) {//ak je hrac platny
        game->state.snakes[player_id].paused = true;//pozastav hada
        game->pause_countdown[player_id] = 0;//vynuluj countdown
    }
    
    pthread_mutex_unlock(&game->mutex);//odomkni mutex
}

void resume_player(Game *game, int player_id) {//obnovi pohyb hada hraca
    pthread_mutex_lock(&game->mutex);//zamkni mutex
    
    if (player_id >= 0 && player_id < MAX_PLAYERS && game->client_connected[player_id]) {//ak je hrac platny
        game->pause_countdown[player_id] = 30;//nastav 3 sekundovy countdown (10 ticks/sec)
    }
    
    pthread_mutex_unlock(&game->mutex);//odomkni mutex
}
