#include "game_logic.h"
#include "map.h"
#include "network.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

Game *create_game(const GameConfig *config) {
    Game *game = (Game *)malloc(sizeof(Game));
    if (!game) {
        return NULL;
    }
    
    memset(game, 0, sizeof(Game));
    
    // Initialize game state
    game->state.game_id = rand();
    game->state.player_count = 0;
    game->state.width = config->width;
    game->state.height = config->height;
    game->state.mode = config->mode;
    game->state.time_limit = config->time_limit;
    game->state.elapsed_time = 0;
    game->state.game_over = false;
    game->state.food_count = 0;
    game->state.max_players = config->max_players;
    
    // Load or generate map
    if (config->load_from_file && config->map_file[0] != '\0') {
        int w, h;
        if (load_map_from_file(config->map_file, &game->state.obstacles, &w, &h)) {
            game->state.width = w;
            game->state.height = h;
        } else {
            // Failed to load map, generate random obstacles instead
            Position center = {config->width / 2, config->height / 2};
            int max_attempts = 10;
            bool success = false;
            
            for (int attempt = 0; attempt < max_attempts; attempt++) {
                generate_random_map(&game->state.obstacles, config->width, config->height, 0.10f);
                
                // Verify reachability
                if (is_reachable(game->state.obstacles, config->width, config->height, center)) {
                    success = true;
                    break;
                }
                
                // Free and try again
                free_obstacles(game->state.obstacles);
                game->state.obstacles = NULL;
            }
            
            // If all attempts failed, create empty map
            if (!success) {
                game->state.obstacles = (uint8_t *)calloc(config->width * config->height, sizeof(uint8_t));
            }
        }
    } else if (config->world_type == WORLD_WITH_OBSTACLES) {
        // Generate random map with obstacles - retry until reachable
        Position center = {config->width / 2, config->height / 2};
        int max_attempts = 10;
        bool success = false;
        
        for (int attempt = 0; attempt < max_attempts; attempt++) {
            generate_random_map(&game->state.obstacles, config->width, config->height, 0.10f);
            
            // Verify reachability
            if (is_reachable(game->state.obstacles, config->width, config->height, center)) {
                success = true;
                break;
            }
            
            // Free and try again
            free_obstacles(game->state.obstacles);
            game->state.obstacles = NULL;
        }
        
        // If all attempts failed, create empty map
        if (!success) {
            game->state.obstacles = (uint8_t *)calloc(config->width * config->height, sizeof(uint8_t));
        }
    } else {
        // No obstacles
        game->state.obstacles = (uint8_t *)calloc(config->width * config->height, sizeof(uint8_t));
    }
    
    pthread_mutex_init(&game->mutex, NULL);
    game->running = true;
    game->start_time = time(NULL);
    game->last_player_time = time(NULL);
    game->config = *config; // Store config
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->client_sockets[i] = -1;
        game->client_connected[i] = false;
        game->pause_countdown[i] = 0;
    }
    
    return game;
}

void destroy_game(Game *game) {
    if (!game) {
        return;
    }
    
    game->running = false;
    
    pthread_mutex_lock(&game->mutex);
    
    if (game->state.obstacles) {
        free_obstacles(game->state.obstacles);
    }
    
    pthread_mutex_unlock(&game->mutex);
    pthread_mutex_destroy(&game->mutex);
    
    free(game);
}

int add_player(Game *game, int socket, const char *name) {
    pthread_mutex_lock(&game->mutex);
    
    // Check if game is full based on max_players setting
    if (game->state.player_count >= game->state.max_players) {
        pthread_mutex_unlock(&game->mutex);
        return -1;
    }
    
    // Find free player slot
    int player_id = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!game->client_connected[i]) {
            player_id = i;
            break;
        }
    }
    
    if (player_id == -1) {
        pthread_mutex_unlock(&game->mutex);
        return -1;
    }
    
    // Find spawn position
    int start_x = game->state.width / 2 + (player_id % 4) * 5;
    int start_y = game->state.height / 2 + (player_id / 4) * 5;
    
    // Make sure spawn position is valid
    while (start_x >= game->state.width - 3) start_x -= 5;
    while (start_y >= game->state.height - 3) start_y -= 5;
    
    // Clear the snake slot completely (in case it was used before)
    memset(&game->state.snakes[player_id], 0, sizeof(Snake));
    
    init_snake(&game->state.snakes[player_id], player_id, start_x, start_y, name);
    game->state.snakes[player_id].spawn_time = game->state.elapsed_time; // Set spawn time
    game->client_sockets[player_id] = socket;
    game->client_connected[player_id] = true;
    game->pause_countdown[player_id] = 30; // 3 seconds at 10 ticks/sec
    game->state.player_count++;
    game->last_player_time = time(NULL);
    
    // Generate food for new player
    generate_food(game);
    
    pthread_mutex_unlock(&game->mutex);
    
    return player_id;
}

void remove_player(Game *game, int player_id) {
    pthread_mutex_lock(&game->mutex);
    
    if (player_id >= 0 && player_id < MAX_PLAYERS && game->client_connected[player_id]) {
        game->state.snakes[player_id].alive = false;
        game->client_connected[player_id] = false;
        game->state.player_count--;
        game->last_player_time = time(NULL);
    }
    
    pthread_mutex_unlock(&game->mutex);
}

void generate_food(Game *game) {
    // Generate food equal to number of active players
    int target_food = 0;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->client_connected[i] && game->state.snakes[i].alive) {
            target_food++;
        }
    }
    
    while (game->state.food_count < target_food) {
        Position food_pos;
        bool valid = false;
        int attempts = 0;
        
        while (!valid && attempts < 100) {
            food_pos.x = rand() % game->state.width;
            food_pos.y = rand() % game->state.height;
            
            // Check if position is valid
            valid = is_valid_position(game, food_pos);
            
            // Check if food already exists here
            for (int i = 0; i < game->state.food_count && valid; i++) {
                if (game->state.food[i].x == food_pos.x && game->state.food[i].y == food_pos.y) {
                    valid = false;
                }
            }
            
            attempts++;
        }
        
        if (valid) {
            game->state.food[game->state.food_count++] = food_pos;
        } else {
            break;
        }
    }
}

bool is_valid_position(Game *game, Position pos) {
    // Check bounds
    if (pos.x < 0 || pos.x >= game->state.width || pos.y < 0 || pos.y >= game->state.height) {
        return false;
    }
    
    // Check obstacles
    if (game->state.obstacles && is_obstacle(game->state.obstacles, pos.x, pos.y, game->state.width)) {
        return false;
    }
    
    // Check snakes
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->client_connected[i] && is_position_on_snake(&game->state.snakes[i], pos)) {
            return false;
        }
    }
    
    return true;
}

void update_game(Game *game) {
    pthread_mutex_lock(&game->mutex);
    
    // Update elapsed time
    game->state.elapsed_time = (int)(time(NULL) - game->start_time);
    
    // Check timed mode
    if (game->state.mode == MODE_TIMED && game->state.time_limit > 0) {
        if (game->state.elapsed_time >= game->state.time_limit) {
            game->state.game_over = true;
            pthread_mutex_unlock(&game->mutex);
            return;
        }
    }
    
    // Check standard mode (10 seconds without players)
    if (game->state.mode == MODE_STANDARD && game->state.player_count == 0) {
        if (time(NULL) - game->last_player_time >= 10) {
            game->state.game_over = true;
            pthread_mutex_unlock(&game->mutex);
            return;
        }
    }
    
    // Update pause countdowns (only for resume countdown)
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->pause_countdown[i] > 0) {
            game->pause_countdown[i]--;
            if (game->pause_countdown[i] == 0) {
                game->state.snakes[i].paused = false;
            }
        }
    }
    
    // Move snakes
    bool wrap_around = (game->state.obstacles == NULL || 
                        is_obstacle(game->state.obstacles, 0, 0, game->state.width) == false);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->client_connected[i] && game->state.snakes[i].alive && !game->state.snakes[i].paused) {
            move_snake(&game->state.snakes[i], game->state.width, game->state.height, wrap_around);
        }
    }
    
    // Check collisions
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!game->client_connected[i] || !game->state.snakes[i].alive) {
            continue;
        }
        
        Position head = game->state.snakes[i].positions[0];
        
        // Check wall collision (if not wrapping)
        if (!wrap_around) {
            if (head.x < 0 || head.x >= game->state.width || 
                head.y < 0 || head.y >= game->state.height) {
                game->state.snakes[i].alive = false;
                continue;
            }
        }
        
        // Check obstacle collision
        if (game->state.obstacles && is_obstacle(game->state.obstacles, head.x, head.y, game->state.width)) {
            game->state.snakes[i].alive = false;
            continue;
        }
        
        // Check self collision
        if (check_self_collision(&game->state.snakes[i])) {
            game->state.snakes[i].alive = false;
            continue;
        }
        
        // Check collision with other snakes
        for (int j = 0; j < MAX_PLAYERS; j++) {
            if (i != j && game->client_connected[j]) {
                if (check_collision_with_snake(&game->state.snakes[i], &game->state.snakes[j])) {
                    game->state.snakes[i].alive = false;
                    break;
                }
            }
        }
        
        // Check food collision
        for (int f = 0; f < game->state.food_count; f++) {
            if (head.x == game->state.food[f].x && head.y == game->state.food[f].y) {
                grow_snake(&game->state.snakes[i]);
                // Remove this food and shift array
                for (int k = f; k < game->state.food_count - 1; k++) {
                    game->state.food[k] = game->state.food[k + 1];
                }
                game->state.food_count--;
                break;
            }
        }
    }
    
    // Generate food if needed
    generate_food(game);
    
    pthread_mutex_unlock(&game->mutex);
}

void broadcast_game_state(Game *game) {
    pthread_mutex_lock(&game->mutex);
    
    Message msg;
    msg.type = MSG_GAME_STATE;
    msg.player_id = -1;
    msg.data.state = game->state;
    
    uint8_t buffer[BUFFER_SIZE];
    size_t size;
    serialize_message(&msg, buffer, &size);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (game->client_connected[i] && game->client_sockets[i] >= 0) {
            send_data(game->client_sockets[i], buffer, size);
        }
    }
    
    pthread_mutex_unlock(&game->mutex);
}

void handle_player_input(Game *game, int player_id, Direction direction) {
    pthread_mutex_lock(&game->mutex);
    
    if (player_id >= 0 && player_id < MAX_PLAYERS && game->client_connected[player_id]) {
        change_direction(&game->state.snakes[player_id], direction);
    }
    
    pthread_mutex_unlock(&game->mutex);
}

void pause_player(Game *game, int player_id) {
    pthread_mutex_lock(&game->mutex);
    
    if (player_id >= 0 && player_id < MAX_PLAYERS && game->client_connected[player_id]) {
        game->state.snakes[player_id].paused = true;
        game->pause_countdown[player_id] = 0; // Reset countdown when pausing
    }
    
    pthread_mutex_unlock(&game->mutex);
}

void resume_player(Game *game, int player_id) {
    pthread_mutex_lock(&game->mutex);
    
    if (player_id >= 0 && player_id < MAX_PLAYERS && game->client_connected[player_id]) {
        game->pause_countdown[player_id] = 30; // 3 seconds
    }
    
    pthread_mutex_unlock(&game->mutex);
}
