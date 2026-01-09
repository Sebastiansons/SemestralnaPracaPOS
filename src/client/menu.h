#ifndef MENU_H
#define MENU_H

#include "protocol.h"
#include <stdbool.h>

typedef enum {
    MENU_NEW_GAME,
    MENU_JOIN_GAME,
    MENU_RESUME_GAME,
    MENU_EXIT,
    MENU_CANCEL
} MenuChoice;

// Menu functions
MenuChoice show_main_menu(bool can_resume);
bool get_game_config(GameConfig *config, int *port);
bool get_connection_info(char *host, int *port, char *player_name);
void show_error(const char *message);
void show_game_over_stats(const GameState *state);

#endif // MENU_H
