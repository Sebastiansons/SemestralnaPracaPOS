#include "protocol.h"
#include <string.h>
#include <stdio.h>

void serialize_message(const Message *msg, uint8_t *buffer, size_t *size) {
    size_t offset = 0;
    
    // Message type
    memcpy(buffer + offset, &msg->type, sizeof(MessageType));
    offset += sizeof(MessageType);
    
    // Player ID
    memcpy(buffer + offset, &msg->player_id, sizeof(int));
    offset += sizeof(int);
    
    // Data based on type
    switch (msg->type) {
        case MSG_CREATE_GAME:
            memcpy(buffer + offset, &msg->data.config, sizeof(GameConfig));
            offset += sizeof(GameConfig);
            break;
            
        case MSG_JOIN_GAME:
            memcpy(buffer + offset, &msg->data.join_info, sizeof(msg->data.join_info));
            offset += sizeof(msg->data.join_info);
            break;
            
        case MSG_GAME_STATE: {
            // Serialize game state without the pointer
            GameState *state = &msg->data.state;
            
            // Copy basic fields
            memcpy(buffer + offset, &state->game_id, sizeof(int));
            offset += sizeof(int);
            
            memcpy(buffer + offset, state->snakes, sizeof(Snake) * MAX_PLAYERS);
            offset += sizeof(Snake) * MAX_PLAYERS;
            
            memcpy(buffer + offset, &state->player_count, sizeof(int));
            offset += sizeof(int);
            
            memcpy(buffer + offset, state->food, sizeof(Position) * MAX_PLAYERS);
            offset += sizeof(Position) * MAX_PLAYERS;
            
            memcpy(buffer + offset, &state->food_count, sizeof(int));
            offset += sizeof(int);
            
            // Serialize obstacles array
            memcpy(buffer + offset, &state->width, sizeof(int));
            offset += sizeof(int);
            
            memcpy(buffer + offset, &state->height, sizeof(int));
            offset += sizeof(int);
            
            int obstacle_size = state->width * state->height;
            if (state->obstacles && obstacle_size > 0) {
                memcpy(buffer + offset, state->obstacles, obstacle_size);
            } else {
                memset(buffer + offset, 0, obstacle_size);
            }
            offset += obstacle_size;
            
            memcpy(buffer + offset, &state->elapsed_time, sizeof(int));
            offset += sizeof(int);
            
            memcpy(buffer + offset, &state->time_limit, sizeof(int));
            offset += sizeof(int);
            
            memcpy(buffer + offset, &state->mode, sizeof(GameMode));
            offset += sizeof(GameMode);
            
            memcpy(buffer + offset, &state->game_over, sizeof(bool));
            offset += sizeof(bool);
            
            memcpy(buffer + offset, &state->max_players, sizeof(int));
            offset += sizeof(int);
            break;
        }
            
        case MSG_PLAYER_INPUT:
            memcpy(buffer + offset, &msg->data.direction, sizeof(Direction));
            offset += sizeof(Direction);
            break;
            
        case MSG_ERROR:
            memcpy(buffer + offset, msg->data.error_msg, 256);
            offset += 256;
            break;
            
        case MSG_PAUSE:
        case MSG_RESUME:
        case MSG_PLAYER_DISCONNECT:
        case MSG_GAME_OVER:
        case MSG_LIST_GAMES:
            // No additional data
            break;
    }
    
    *size = offset;
}

bool deserialize_message(const uint8_t *buffer, size_t size, Message *msg) {
    if (size < sizeof(MessageType) + sizeof(int)) {
        return false;
    }
    
    size_t offset = 0;
    
    // Message type
    memcpy(&msg->type, buffer + offset, sizeof(MessageType));
    offset += sizeof(MessageType);
    
    // Player ID
    memcpy(&msg->player_id, buffer + offset, sizeof(int));
    offset += sizeof(int);
    
    // Data based on type
    switch (msg->type) {
        case MSG_CREATE_GAME:
            if (size < offset + sizeof(GameConfig)) return false;
            memcpy(&msg->data.config, buffer + offset, sizeof(GameConfig));
            break;
            
        case MSG_JOIN_GAME:
            if (size < offset + sizeof(msg->data.join_info)) return false;
            memcpy(&msg->data.join_info, buffer + offset, sizeof(msg->data.join_info));
            break;
            
        case MSG_GAME_STATE: {
            // Deserialize game state
            GameState *state = &msg->data.state;
            
            if (size < offset + sizeof(int)) return false;
            memcpy(&state->game_id, buffer + offset, sizeof(int));
            offset += sizeof(int);
            
            if (size < offset + sizeof(Snake) * MAX_PLAYERS) return false;
            memcpy(state->snakes, buffer + offset, sizeof(Snake) * MAX_PLAYERS);
            offset += sizeof(Snake) * MAX_PLAYERS;
            
            if (size < offset + sizeof(int)) return false;
            memcpy(&state->player_count, buffer + offset, sizeof(int));
            offset += sizeof(int);
            
            if (size < offset + sizeof(Position) * MAX_PLAYERS) return false;
            memcpy(state->food, buffer + offset, sizeof(Position) * MAX_PLAYERS);
            offset += sizeof(Position) * MAX_PLAYERS;
            
            if (size < offset + sizeof(int)) return false;
            memcpy(&state->food_count, buffer + offset, sizeof(int));
            offset += sizeof(int);
            
            if (size < offset + sizeof(int) * 2) return false;
            memcpy(&state->width, buffer + offset, sizeof(int));
            offset += sizeof(int);
            
            memcpy(&state->height, buffer + offset, sizeof(int));
            offset += sizeof(int);
            
            // Allocate and copy obstacles
            int obstacle_size = state->width * state->height;
            if (obstacle_size > 0 && size >= offset + obstacle_size) {
                state->obstacles = (uint8_t *)malloc(obstacle_size);
                if (state->obstacles) {
                    memcpy(state->obstacles, buffer + offset, obstacle_size);
                }
                offset += obstacle_size;
            } else {
                state->obstacles = NULL;
            }
            
            if (size < offset + sizeof(int) * 2) return false;
            memcpy(&state->elapsed_time, buffer + offset, sizeof(int));
            offset += sizeof(int);
            
            memcpy(&state->time_limit, buffer + offset, sizeof(int));
            offset += sizeof(int);
            
            if (size < offset + sizeof(GameMode)) return false;
            memcpy(&state->mode, buffer + offset, sizeof(GameMode));
            offset += sizeof(GameMode);
            
            if (size < offset + sizeof(bool)) return false;
            memcpy(&state->game_over, buffer + offset, sizeof(bool));
            offset += sizeof(bool);
            
            if (size < offset + sizeof(int)) return false;
            memcpy(&state->max_players, buffer + offset, sizeof(int));
            break;
        }
            
        case MSG_PLAYER_INPUT:
            if (size < offset + sizeof(Direction)) return false;
            memcpy(&msg->data.direction, buffer + offset, sizeof(Direction));
            break;
            
        case MSG_ERROR:
            if (size < offset + 256) return false;
            memcpy(msg->data.error_msg, buffer + offset, 256);
            break;
            
        case MSG_PAUSE:
        case MSG_RESUME:
        case MSG_PLAYER_DISCONNECT:
        case MSG_GAME_OVER:
        case MSG_LIST_GAMES:
            // No additional data
            break;
    }
    
    return true;
}
