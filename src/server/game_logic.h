/**
 * @file game_logic.h
 * @brief Core game logic and state management
 * 
 * Manages game state, player connections, game loop updates,
 * food generation, collision detection, and broadcasting state to clients.
 */

#ifndef GAME_LOGIC_H
#define GAME_LOGIC_H

#include "protocol.h"
#include "snake.h"
#include <pthread.h>
#include <stdbool.h>

/** Game update frequency (ticks per second) */
#define TICK_RATE 10

/**
 * @brief Main game structure
 * 
 * Encapsulates all game state, configuration, client connections,
 * and synchronization primitives. Protected by mutex for thread safety.
 */
typedef struct {
    GameState state;                        /**< Current game state */
    GameConfig config;                      /**< Game configuration */
    pthread_mutex_t mutex;                  /**< Mutex for thread-safe access */
    bool running;                           /**< Is game loop running */
    time_t start_time;                      /**< Game start timestamp */
    time_t last_player_time;                /**< Last time a player was connected */
    int client_sockets[MAX_PLAYERS];        /**< Client socket descriptors */
    bool client_connected[MAX_PLAYERS];     /**< Client connection status */
    pthread_t client_threads[MAX_PLAYERS];  /**< Client handler threads */
    int pause_countdown[MAX_PLAYERS];       /**< Countdown after resume/join (ticks) */
} Game;

/**
 * @brief Create new game instance
 * @param config Game configuration
 * @return Pointer to created game, or NULL on failure
 */
Game *create_game(const GameConfig *config);

/**
 * @brief Destroy game and free resources
 * @param game Game instance to destroy
 */
void destroy_game(Game *game);

/**
 * @brief Add player to game
 * @param game Game instance
 * @param socket Client socket descriptor
 * @param name Player name
 * @return Player ID (0-7) on success, -1 on failure
 */
int add_player(Game *game, int socket, const char *name);

/**
 * @brief Remove player from game
 * @param game Game instance
 * @param player_id Player ID to remove
 */
void remove_player(Game *game, int player_id);

/**
 * @brief Update game state for one tick
 * @param game Game instance
 * 
 * Moves snakes, checks collisions, generates food, updates timers,
 * handles pause countdowns, and checks game over conditions.
 */
void update_game(Game *game);

/**
 * @brief Generate food in valid positions
 * @param game Game instance
 * 
 * Generates one food item per active snake at random valid positions.
 */
void generate_food(Game *game);

/**
 * @brief Check if position is valid (not obstacle, within bounds)
 * @param game Game instance
 * @param pos Position to check
 * @return true if valid, false otherwise
 */
bool is_valid_position(Game *game, Position pos);

/**
 * @brief Broadcast current game state to all connected clients
 * @param game Game instance
 * 
 * Thread-safe broadcast using mutex protection.
 */
void broadcast_game_state(Game *game);

/**
 * @brief Handle player input (direction change)
 * @param game Game instance
 * @param player_id Player ID
 * @param direction New direction
 */
void handle_player_input(Game *game, int player_id, Direction direction);

/**
 * @brief Pause player's snake
 * @param game Game instance
 * @param player_id Player ID to pause
 * 
 * Sets paused flag and resets countdown.
 */
void pause_player(Game *game, int player_id);

/**
 * @brief Resume player's snake with 3-second countdown
 * @param game Game instance
 * @param player_id Player ID to resume
 * 
 * Starts 3-second countdown (30 ticks) before snake starts moving.
 */
void resume_player(Game *game, int player_id);

#endif // GAME_LOGIC_H
