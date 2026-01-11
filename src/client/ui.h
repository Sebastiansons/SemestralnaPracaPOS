/**
 * @file ui.h
 * @brief User interface rendering using ncurses
 * 
 * Handles terminal UI initialization, game state rendering, color pairs,
 * and message display.
 */

#ifndef UI_H
#define UI_H

#include "protocol.h"
#include <ncurses.h>
#include <stdbool.h>

/**
 * @brief Initialize ncurses UI
 * @return true on success, false on failure
 * 
 * Initializes ncurses, sets up colors, disables echo, enables keypad.
 */
bool init_ui(void);

/**
 * @brief Cleanup and restore terminal
 * 
 * Ends ncurses mode and restores terminal to normal state.
 */
void cleanup_ui(void);

/**
 * @brief Render current game state
 * @param state Game state to render
 * @param my_player_id Current player's ID
 * @param host Server hostname (unused, for future features)
 * @param port Server port (unused, for future features)
 * 
 * Renders game grid, snakes, food, obstacles, scores, and time.
 */
void render_game_state(const GameState *state, int my_player_id, const char *host, int port);

/**
 * @brief Render centered message
 * @param message Message text to display
 */
void render_message(const char *message);

/**
 * @brief Render death screen with statistics
 * @param score Player's final score
 * @param survival_time Survival time in seconds
 * @param host Server hostname
 * @param port Server port
 * 
 * Shows "YOU DIED" screen with rejoin prompt.
 */
void render_death_message(int score, int survival_time, const char *host, int port);

/**
 * @brief Clear screen
 */
void clear_screen(void);

/**
 * @brief Get ncurses color pair for player
 * @param player_id Player ID (0-7)
 * @return Color pair number for ncurses
 */
int get_color_pair(int player_id);

#endif // UI_H
