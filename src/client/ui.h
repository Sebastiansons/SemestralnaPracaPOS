#ifndef UI_H
#define UI_H

#include "protocol.h"
#include <ncurses.h>
#include <stdbool.h>

// UI functions
bool init_ui(void);
void cleanup_ui(void);
void render_game_state(const GameState *state, int my_player_id);
void render_message(const char *message);
void render_death_message(int score, int survival_time);
void clear_screen(void);
int get_color_pair(int player_id);

#endif // UI_H
