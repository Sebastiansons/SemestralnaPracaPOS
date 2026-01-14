#include "protocol.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void serialize_message(const Message *msg, uint8_t *buffer, size_t *size) {//serializuje Message do binarneho buffera
    size_t offset = 0;//aktualny posun v bufferi
    
    // Message type
    memcpy(buffer + offset, &msg->type, sizeof(MessageType));//skopiruj typ spravy
    offset += sizeof(MessageType);
    
    // Player ID
    memcpy(buffer + offset, &msg->player_id, sizeof(int));//skopiruj ID hraca
    offset += sizeof(int);
    
    // Data based on type
    switch (msg->type) {//serializuj data podla typu spravy
        case MSG_CREATE_GAME:
            memcpy(buffer + offset, &msg->data.config, sizeof(GameConfig));//skopiruj konfiguraciu hry
            offset += sizeof(GameConfig);
            break;
            
        case MSG_JOIN_GAME:
            memcpy(buffer + offset, &msg->data.join_info, sizeof(msg->data.join_info));//skopiruj join info (port, meno)
            offset += sizeof(msg->data.join_info);
            break;
            
        case MSG_GAME_STATE: {//serializuj stav hry
            // Serialize game state without the pointer
            const GameState *state = &msg->data.state;
            
            // Copy basic fields
            memcpy(buffer + offset, &state->game_id, sizeof(int));//ID hry
            offset += sizeof(int);
            
            memcpy(buffer + offset, state->snakes, sizeof(Snake) * MAX_PLAYERS);//vsetky hady
            offset += sizeof(Snake) * MAX_PLAYERS;
            
            memcpy(buffer + offset, &state->player_count, sizeof(int));//pocet hracov
            offset += sizeof(int);
            
            memcpy(buffer + offset, state->food, sizeof(Position) * MAX_PLAYERS);//pozicie jedla
            offset += sizeof(Position) * MAX_PLAYERS;
            
            memcpy(buffer + offset, &state->food_count, sizeof(int));//pocet jedla
            offset += sizeof(int);
            
            // Serialize obstacles array
            memcpy(buffer + offset, &state->width, sizeof(int));//sirka mapy
            offset += sizeof(int);
            
            memcpy(buffer + offset, &state->height, sizeof(int));//vyska mapy
            offset += sizeof(int);
            
            int obstacle_size = state->width * state->height;//velkost bitmapy prekazok
            if (state->obstacles && obstacle_size > 0) {
                memcpy(buffer + offset, state->obstacles, obstacle_size);//skopiruj prekazky
            } else {
                memset(buffer + offset, 0, obstacle_size);//alebo vynuluj
            }
            offset += obstacle_size;
            
            memcpy(buffer + offset, &state->elapsed_time, sizeof(int));//uplynuly cas
            offset += sizeof(int);
            
            memcpy(buffer + offset, &state->time_limit, sizeof(int));//casovy limit
            offset += sizeof(int);
            
            memcpy(buffer + offset, &state->mode, sizeof(GameMode));//mod hry
            offset += sizeof(GameMode);
            
            memcpy(buffer + offset, &state->game_over, sizeof(bool));//ci je hra ukoncena
            offset += sizeof(bool);
            
            memcpy(buffer + offset, &state->max_players, sizeof(int));//max pocet hracov
            offset += sizeof(int);
            break;
        }
            
        case MSG_PLAYER_INPUT:
            memcpy(buffer + offset, &msg->data.direction, sizeof(Direction));//skopiruj smer pohybu
            offset += sizeof(Direction);
            break;
            
        case MSG_ERROR:
            memcpy(buffer + offset, msg->data.error_msg, 256);//skopiruj chybovu spravu
            offset += 256;
            break;
            
        case MSG_PAUSE:
        case MSG_RESUME:
        case MSG_PLAYER_DISCONNECT:
        case MSG_GAME_OVER:
        case MSG_LIST_GAMES:
            // No additional data - tieto spravy nemaju ziadne dalsie data
            break;
    }
    
    *size = offset;//vrat celkovu velkost serializovanej spravy
}

bool deserialize_message(const uint8_t *buffer, size_t size, Message *msg) {//deserializuje binarny buffer do Message struktury
    if (size < sizeof(MessageType) + sizeof(int)) {//minimalna velkost spravy
        return false;
    }
    
    size_t offset = 0;//aktualny posun v bufferi
    
    // Message type
    memcpy(&msg->type, buffer + offset, sizeof(MessageType));//nacitaj typ spravy
    offset += sizeof(MessageType);
    
    // Player ID
    memcpy(&msg->player_id, buffer + offset, sizeof(int));//nacitaj ID hraca
    offset += sizeof(int);
    
    // Data based on type
    switch (msg->type) {//deserializuj data podla typu spravy
        case MSG_CREATE_GAME:
            if (size < offset + sizeof(GameConfig)) return false;//over velkost bufferu
            memcpy(&msg->data.config, buffer + offset, sizeof(GameConfig));//nacitaj konfiguraciu
            break;
            
        case MSG_JOIN_GAME:
            if (size < offset + sizeof(msg->data.join_info)) return false;//over velkost bufferu
            memcpy(&msg->data.join_info, buffer + offset, sizeof(msg->data.join_info));//nacitaj join info
            break;
            
        case MSG_GAME_STATE: {//deserializuj stav hry
            // Deserialize game state
            GameState *state = &msg->data.state;
            
            if (size < offset + sizeof(int)) return false;//over velkost
            memcpy(&state->game_id, buffer + offset, sizeof(int));//nacitaj ID hry
            offset += sizeof(int);
            
            if (size < offset + sizeof(Snake) * MAX_PLAYERS) return false;//over velkost
            memcpy(state->snakes, buffer + offset, sizeof(Snake) * MAX_PLAYERS);//nacitaj vsetkych hadov
            offset += sizeof(Snake) * MAX_PLAYERS;
            
            if (size < offset + sizeof(int)) return false;//over velkost
            memcpy(&state->player_count, buffer + offset, sizeof(int));//nacitaj pocet hracov
            offset += sizeof(int);
            
            if (size < offset + sizeof(Position) * MAX_PLAYERS) return false;//over velkost
            memcpy(state->food, buffer + offset, sizeof(Position) * MAX_PLAYERS);//nacitaj pozicie jedla
            offset += sizeof(Position) * MAX_PLAYERS;
            
            if (size < offset + sizeof(int)) return false;//over velkost
            memcpy(&state->food_count, buffer + offset, sizeof(int));//nacitaj pocet jedla
            offset += sizeof(int);
            
            if (size < offset + sizeof(int) * 2) return false;//over velkost
            memcpy(&state->width, buffer + offset, sizeof(int));//nacitaj sirku mapy
            offset += sizeof(int);
            
            memcpy(&state->height, buffer + offset, sizeof(int));//nacitaj vysku mapy
            offset += sizeof(int);
            
            // Allocate and copy obstacles
            int obstacle_size = state->width * state->height;//velkost bitmapy prekazok
            if (obstacle_size > 0 && size >= offset + obstacle_size) {
                state->obstacles = (uint8_t *)malloc(obstacle_size);//alokuj pamat pre prekazky
                if (state->obstacles) {
                    memcpy(state->obstacles, buffer + offset, obstacle_size);//skopiruj prekazky
                }
                offset += obstacle_size;
            } else {
                state->obstacles = NULL;//ziadne prekazky
            }
            
            if (size < offset + sizeof(int) * 2) return false;//over velkost
            memcpy(&state->elapsed_time, buffer + offset, sizeof(int));//nacitaj uplynuly cas
            offset += sizeof(int);
            
            memcpy(&state->time_limit, buffer + offset, sizeof(int));//nacitaj casovy limit
            offset += sizeof(int);
            
            if (size < offset + sizeof(GameMode)) return false;//over velkost
            memcpy(&state->mode, buffer + offset, sizeof(GameMode));//nacitaj mod hry
            offset += sizeof(GameMode);
            
            if (size < offset + sizeof(bool)) return false;//over velkost
            memcpy(&state->game_over, buffer + offset, sizeof(bool));//nacitaj ci je hra ukoncena
            offset += sizeof(bool);
            
            if (size < offset + sizeof(int)) return false;//over velkost
            memcpy(&state->max_players, buffer + offset, sizeof(int));//nacitaj max pocet hracov
            break;
        }
            
        case MSG_PLAYER_INPUT:
            if (size < offset + sizeof(Direction)) return false;//over velkost
            memcpy(&msg->data.direction, buffer + offset, sizeof(Direction));//nacitaj smer pohybu
            break;
            
        case MSG_ERROR:
            if (size < offset + 256) return false;//over velkost
            memcpy(msg->data.error_msg, buffer + offset, 256);//nacitaj chybovu spravu
            break;
            
        case MSG_PAUSE:
        case MSG_RESUME:
        case MSG_PLAYER_DISCONNECT:
        case MSG_GAME_OVER:
        case MSG_LIST_GAMES:
            // No additional data - tieto spravy nemaju ziadne dalsie data
            break;
    }
    
    return true;//deserializacia uspesna
}
