#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MAX_PLAYERS 8
#define MAX_NAME_LENGTH 32
#define MAX_SNAKE_LENGTH 1000

// Message types
typedef enum {
    MSG_CREATE_GAME,
    MSG_JOIN_GAME,
    MSG_GAME_STATE,
    MSG_PLAYER_INPUT,
    MSG_PLAYER_DISCONNECT,
    MSG_GAME_OVER,
    MSG_ERROR,
    MSG_PAUSE,
    MSG_RESUME,
    MSG_LIST_GAMES
} MessageType;

// Game mode
typedef enum {
    MODE_STANDARD,
    MODE_TIMED
} GameMode;

// World type
typedef enum {
    WORLD_NO_OBSTACLES,
    WORLD_WITH_OBSTACLES
} WorldType;

// Direction
typedef enum {
    DIR_UP,
    DIR_DOWN,
    DIR_LEFT,
    DIR_RIGHT,
    DIR_NONE
} Direction;

// Cell type
typedef enum {
    CELL_EMPTY,
    CELL_SNAKE,
    CELL_FOOD,
    CELL_OBSTACLE
} CellType;

// Position
typedef struct {
    int x;
    int y;
} Position;

// Snake segment
typedef struct {
    Position positions[MAX_SNAKE_LENGTH];
    int length;
    Direction direction;
    Direction pending_direction; // Next direction to apply
    int player_id;
    int score;
    bool alive;
    bool paused;
    char name[MAX_NAME_LENGTH];
    int spawn_time; // Time when player joined (in seconds)
} Snake;

// Game configuration
typedef struct {
    GameMode mode;
    WorldType world_type;
    int width;
    int height;
    int time_limit; // in seconds, 0 for unlimited
    char map_file[256];
    bool load_from_file;
    int max_players; // 1 for singleplayer, 2-8 for multiplayer
} GameConfig;

// Game state
typedef struct {
    int game_id;
    Snake snakes[MAX_PLAYERS];
    int player_count;
    Position food[MAX_PLAYERS];
    int food_count;
    uint8_t *obstacles; // width * height bitmap
    int width;
    int height;
    int elapsed_time;
    int time_limit;
    GameMode mode;
    bool game_over;
    int max_players; // Maximum allowed players for this game
} GameState;

// Message structures
typedef struct {
    MessageType type;
    int player_id;
    union {
        GameConfig config;
        GameState state;
        Direction direction;
        char error_msg[256];
        struct {
            int port;
            char name[MAX_NAME_LENGTH];
        } join_info;
    } data;
} Message;

// Protocol functions
void serialize_message(const Message *msg, uint8_t *buffer, size_t *size);
bool deserialize_message(const uint8_t *buffer, size_t size, Message *msg);

#endif // PROTOCOL_H
