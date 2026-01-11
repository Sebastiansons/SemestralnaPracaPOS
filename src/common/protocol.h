/**
 * @file protocol.h
 * @brief Protocol definitions for client-server communication
 * 
 * Defines all message types, data structures, and serialization functions
 * for the Snake Game client-server protocol.
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** Maximum number of players in a game */
#define MAX_PLAYERS 8

/** Maximum length of player name */
#define MAX_NAME_LENGTH 32

/** Maximum length of a snake */
#define MAX_SNAKE_LENGTH 1000

/**
 * @brief Message types for client-server communication
 */
typedef enum {
    MSG_CREATE_GAME,         /**< Client requests to create new game */
    MSG_JOIN_GAME,           /**< Client requests to join existing game */
    MSG_GAME_STATE,          /**< Server broadcasts current game state */
    MSG_PLAYER_INPUT,        /**< Client sends player input (direction) */
    MSG_PLAYER_DISCONNECT,   /**< Client disconnects from game */
    MSG_GAME_OVER,           /**< Server notifies game is over */
    MSG_ERROR,               /**< Server sends error message */
    MSG_PAUSE,               /**< Client pauses game */
    MSG_RESUME,              /**< Client resumes game */
    MSG_LIST_GAMES           /**< Client requests list of active games */
} MessageType;

/**
 * @brief Game mode types
 */
typedef enum {
    MODE_STANDARD,  /**< Game ends 10s after last player leaves */
    MODE_TIMED      /**< Game ends after specified time limit */
} GameMode;

/**
 * @brief World types
 */
typedef enum {
    WORLD_NO_OBSTACLES,    /**< No obstacles, snake wraps around edges */
    WORLD_WITH_OBSTACLES   /**< Obstacles present, no wrapping */
} WorldType;

/**
 * @brief Snake movement directions
 */
typedef enum {
    DIR_UP,      /**< Move up */
    DIR_DOWN,    /**< Move down */
    DIR_LEFT,    /**< Move left */
    DIR_RIGHT,   /**< Move right */
    DIR_NONE     /**< No movement (initial state) */
} Direction;

/**
 * @brief Cell types in game grid
 */
typedef enum {
    CELL_EMPTY,     /**< Empty cell */
    CELL_SNAKE,     /**< Cell occupied by snake */
    CELL_FOOD,      /**< Cell with food */
    CELL_OBSTACLE   /**< Cell with obstacle */
} CellType;

/**
 * @brief 2D position in game grid
 */
typedef struct {
    int x;  /**< X coordinate */
    int y;  /**< Y coordinate */
} Position;

/**
 * @brief Snake data structure
 */
typedef struct {
    Position positions[MAX_SNAKE_LENGTH];  /**< Snake body positions */
    int length;                             /**< Current snake length */
    Direction direction;                    /**< Current direction */
    Direction pending_direction;            /**< Next direction to apply */
    int player_id;                          /**< Player ID (0-7) */
    int score;                              /**< Player score */
    bool alive;                             /**< Is snake alive */
    bool paused;                            /**< Is snake paused */
    char name[MAX_NAME_LENGTH];             /**< Player name */
    int spawn_time;                         /**< Spawn time in seconds */
} Snake;

/**
 * @brief Game configuration
 */
typedef struct {
    GameMode mode;            /**< Game mode (standard/timed) */
    WorldType world_type;     /**< World type (with/without obstacles) */
    int width;                /**< World width (20-200) */
    int height;               /**< World height (10-100) */
    int time_limit;           /**< Time limit in seconds (0 = unlimited) */
    char map_file[256];       /**< Map file path */
    bool load_from_file;      /**< Load map from file? */
    int max_players;          /**< Max players (1-8) */
} GameConfig;

/**
 * @brief Current game state
 * 
 * Broadcasted by server to all clients every tick.
 */
typedef struct {
    int game_id;                    /**< Unique game ID */
    Snake snakes[MAX_PLAYERS];      /**< All snakes in game */
    int player_count;               /**< Number of active players */
    Position food[MAX_PLAYERS];     /**< Food positions */
    int food_count;                 /**< Number of food items */
    uint8_t *obstacles;             /**< Obstacle bitmap (width * height) */
    int width;                      /**< World width */
    int height;                     /**< World height */
    int elapsed_time;               /**< Elapsed time in seconds */
    int time_limit;                 /**< Time limit in seconds */
    GameMode mode;                  /**< Game mode */
    bool game_over;                 /**< Is game over */
    int max_players;                /**< Max allowed players */
} GameState;

/**
 * @brief Message structure for client-server communication
 * 
 * Uses union for different message data types.
 */
typedef struct {
    MessageType type;   /**< Message type */
    int player_id;      /**< Sender player ID (-1 if not applicable) */
    union {
        GameConfig config;      /**< Game configuration (MSG_CREATE_GAME) */
        GameState state;        /**< Game state (MSG_GAME_STATE) */
        Direction direction;    /**< Player input (MSG_PLAYER_INPUT) */
        char error_msg[256];    /**< Error message (MSG_ERROR) */
        struct {
            int port;                       /**< Server port */
            char name[MAX_NAME_LENGTH];     /**< Player name */
        } join_info;            /**< Join info (MSG_JOIN_GAME) */
    } data;
} Message;

/**
 * @brief Serialize message to binary format
 * @param msg Message to serialize
 * @param buffer Output buffer
 * @param size Output size in bytes
 */
void serialize_message(const Message *msg, uint8_t *buffer, size_t *size);

/**
 * @brief Deserialize message from binary format
 * @param buffer Input buffer
 * @param size Input size in bytes
 * @param msg Output message
 * @return true if successful, false otherwise
 */
bool deserialize_message(const uint8_t *buffer, size_t size, Message *msg);

#endif // PROTOCOL_H
