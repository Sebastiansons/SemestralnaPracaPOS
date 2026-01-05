#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "protocol.h"
#include "snake.h"
#include <pthread.h>
#include <stdbool.h>

#define TICK_RATE 10 // Ticks per second

typedef struct {
    GameState state;
    pthread_mutex_t mutex;
    bool running;
    time_t start_time;
    time_t last_player_time;
    int client_sockets[MAX_PLAYERS];
    bool client_connected[MAX_PLAYERS];
    pthread_t client_threads[MAX_PLAYERS];
    int pause_countdown[MAX_PLAYERS]; // Countdown after resume/join
} Game;

// Game functions
Game *create_game(const GameConfig *config);
void destroy_game(Game *game);
int add_player(Game *game, int socket, const char *name);
void remove_player(Game *game, int player_id);
void update_game(Game *game);
void generate_food(Game *game);
bool is_valid_position(Game *game, Position pos);
void broadcast_game_state(Game *game);
void handle_player_input(Game *game, int player_id, Direction direction);
void pause_player(Game *game, int player_id);
void resume_player(Game *game, int player_id);

#endif // GAME_LOGIC_H
