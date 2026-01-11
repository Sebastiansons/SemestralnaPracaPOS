/**
 * @file menu.h
 * @brief Menu system for game configuration and navigation
 * 
 * Provides main menu, game configuration dialogs, connection dialogs,
 * error displays, and game over statistics.
 */

#ifndef MENU_H
#define MENU_H

#include "protocol.h"
#include <stdbool.h>

/**
 * @brief Menu selection choices
 */
typedef enum {
    MENU_NEW_GAME,      /**< Create new game */
    MENU_JOIN_GAME,     /**< Join existing game */
    MENU_RESUME_GAME,   /**< Resume paused game */
    MENU_EXIT,          /**< Exit application */
    MENU_CANCEL         /**< Cancel current action */
} MenuChoice;

/**
 * @brief Show main menu
 * @param can_resume Enable "Resume Game" option
 * @return Selected menu choice
 * 
 * Displays ASCII art title and menu options. Blocks until user selects.
 */
MenuChoice show_main_menu(bool can_resume);

/**
 * @brief Get game configuration from user
 * @param config Output game configuration
 * @param port Output server port
 * @return true if configuration completed, false if cancelled
 * 
 * Interactive dialog for setting:
 * - World type (with/without obstacles)
 * - Map file (if obstacles)
 * - World dimensions (20-200 × 10-100)
 * - Game mode (standard/timed)
 * - Player mode (single/multiplayer)
 * - Server port
 */
bool get_game_config(GameConfig *config, int *port);

/**
 * @brief Get connection info for joining game
 * @param port Output server port
 * @param player_name Output player name
 * @return true if info provided, false if cancelled
 * 
 * Prompts for player name and server port.
 */
bool get_connection_info(int *port, char *player_name);

/**
 * @brief Show error message dialog
 * @param message Error message text
 * 
 * Displays error in red box, waits 3 seconds.
 */
void show_error(const char *message);

/**
 * @brief Show game over statistics
 * @param state Final game state
 * 
 * Displays all players' scores and survival times.
 */
void show_game_over_stats(const GameState *state);

#endif // MENU_H
