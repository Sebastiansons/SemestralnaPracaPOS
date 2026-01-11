#include "menu.h"
#include "ui.h"
#include "network.h"
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>

#define COLOR_PAIR_FOOD 9

MenuChoice show_main_menu(bool can_resume) {
    clear();
    nodelay(stdscr, FALSE);
    
    const char *title = "=== SNAKE GAME ===";
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int start_y = max_y / 2 - 5;
    int start_x = max_x / 2 - strlen(title) / 2;
    
    attron(A_BOLD);
    mvprintw(start_y, start_x, "%s", title);
    attroff(A_BOLD);
    
    start_y += 2;
    
    // Dynamically build menu based on can_resume
    if (can_resume) {
        mvprintw(start_y++, start_x, "1. New Game");
        mvprintw(start_y++, start_x, "2. Join Game");
        mvprintw(start_y++, start_x, "3. Resume Game");
        mvprintw(start_y++, start_x, "4. Exit");
    } else {
        mvprintw(start_y++, start_x, "1. New Game");
        mvprintw(start_y++, start_x, "2. Join Game");
        mvprintw(start_y++, start_x, "3. Exit");
    }
    
    mvprintw(start_y + 2, start_x - 10, "Enter your choice: ");
    refresh();
    
    int choice = getch();
    
    nodelay(stdscr, TRUE);
    
    if (can_resume) {
        switch (choice) {
            case '1': return MENU_NEW_GAME;
            case '2': return MENU_JOIN_GAME;
            case '3': return MENU_RESUME_GAME;
            case '4': return MENU_EXIT;
            default: return MENU_CANCEL;
        }
    } else {
        switch (choice) {
            case '1': return MENU_NEW_GAME;
            case '2': return MENU_JOIN_GAME;
            case '3': return MENU_EXIT;
            default: return MENU_CANCEL;
        }
    }
}

bool get_game_config(GameConfig *config, int *port) {
    clear();
    nodelay(stdscr, FALSE);
    echo();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_y = max_y / 2 - 10;
    int start_x = max_x / 2 - 25;
    
    // Title
    attron(A_BOLD | A_UNDERLINE);
    mvprintw(start_y, start_x + 5, "=== NEW GAME CONFIGURATION ===");
    attroff(A_BOLD | A_UNDERLINE);
    start_y += 3;
    
    // Mode
    attron(A_BOLD);
    mvprintw(start_y, start_x, "Game mode:");
    attroff(A_BOLD);
    mvprintw(start_y + 1, start_x + 2, "1 = Standard (10s after last player) [default]");
    mvprintw(start_y + 2, start_x + 2, "2 = Timed (fixed time limit)");
    mvprintw(start_y + 3, start_x, "Choice: ");
    move(start_y + 3, start_x + 8);
    refresh();
    char mode_str[10];
    getnstr(mode_str, 9);
    flushinp();  // Clear input buffer
    int mode_choice = atoi(mode_str);
    if (mode_choice != 1 && mode_choice != 2) {
        mode_choice = 1; // Default to Standard
    }
    config->mode = (mode_choice == 2) ? MODE_TIMED : MODE_STANDARD;
    start_y += 5;
    
    // Time limit
    if (config->mode == MODE_TIMED) {
        attron(A_BOLD);
        mvprintw(start_y, start_x, "Time limit (seconds):");
        attroff(A_BOLD);
        mvprintw(start_y, start_x + 22, "[default: 300] ");
        move(start_y, start_x + 37);
        refresh();
        char time_str[10];
        getnstr(time_str, 9);
        flushinp();  // Clear input buffer
        config->time_limit = atoi(time_str);
        if (config->time_limit <= 0) config->time_limit = 300;
        start_y += 2;
    } else {
        config->time_limit = 0;
    }
    
    // World type
    attron(A_BOLD);
    mvprintw(start_y, start_x, "World type:");
    attroff(A_BOLD);
    mvprintw(start_y + 1, start_x + 2, "1 = No obstacles (wrap around) [default]");
    mvprintw(start_y + 2, start_x + 2, "2 = With obstacles");
    mvprintw(start_y + 3, start_x, "Choice: ");
    move(start_y + 3, start_x + 8);
    refresh();
    char world_str[10];
    getnstr(world_str, 9);
    flushinp();  // Clear input buffer
    int world_choice = atoi(world_str);
    if (world_choice != 1 && world_choice != 2) {
        world_choice = 1; // Default to No obstacles
    }
    config->world_type = (world_choice == 2) ? WORLD_WITH_OBSTACLES : WORLD_NO_OBSTACLES;
    start_y += 5;
    
    // Load from file
    config->load_from_file = false;
    if (config->world_type == WORLD_WITH_OBSTACLES) {
        attron(A_BOLD);
        mvprintw(start_y, start_x, "Load map from file?");
        attroff(A_BOLD);
        mvprintw(start_y + 1, start_x + 2, "y = Load from file");
        mvprintw(start_y + 2, start_x + 2, "n = Generate random obstacles [default]");
        mvprintw(start_y + 3, start_x, "Choice: ");
        move(start_y + 3, start_x + 8);
        refresh();
        char load_str[10];
        getnstr(load_str, 9);
        flushinp();  // Clear input buffer
        start_y += 5;
        
        if (load_str[0] == 'y' || load_str[0] == 'Y') {
            config->load_from_file = true;
            attron(A_BOLD);
            mvprintw(start_y, start_x, "Map file path:");
            attroff(A_BOLD);
            mvprintw(start_y, start_x + 15, " ");
            move(start_y, start_x + 16);
            refresh();
            getnstr(config->map_file, sizeof(config->map_file) - 1);
            flushinp();  // Clear input buffer
            start_y += 2;
        }
    }
    
    // Dimensions
    if (!config->load_from_file) {
        attron(A_BOLD);
        mvprintw(start_y, start_x, "World width (20-200):");
        attroff(A_BOLD);
        mvprintw(start_y, start_x + 22, "[default: 40] ");
        move(start_y, start_x + 36);
        refresh();
        char width_str[10];
        getnstr(width_str, 9);
        flushinp();  // Clear input buffer
        config->width = atoi(width_str);
        if (config->width < 20 || config->width > 200) {
            config->width = 40; // Default if out of range
        }
        start_y += 2;
        
        attron(A_BOLD);
        mvprintw(start_y, start_x, "World height (10-100):");
        attroff(A_BOLD);
        mvprintw(start_y, start_x + 23, "[default: 20] ");
        move(start_y, start_x + 37);
        refresh();
        char height_str[10];
        getnstr(height_str, 9);
        flushinp();  // Clear input buffer
        config->height = atoi(height_str);
        if (config->height < 10 || config->height > 100) {
            config->height = 20; // Default if out of range
        }
        start_y += 2;
    } else {
        config->width = 40;
        config->height = 20;
    }
    
    // Player mode (Singleplayer/Multiplayer)
    attron(A_BOLD);
    mvprintw(start_y, start_x, "Player mode:");
    attroff(A_BOLD);
    mvprintw(start_y + 1, start_x + 2, "1 = Singleplayer (only you) [default]");
    mvprintw(start_y + 2, start_x + 2, "2 = Multiplayer (up to 8 players)");
    mvprintw(start_y + 3, start_x, "Choice: ");
    mvprintw(start_y + 3, start_x + 8, " ");
    move(start_y + 3, start_x + 9);
    refresh();
    char player_mode_str[10];
    getnstr(player_mode_str, 9);
    flushinp();  // Clear input buffer
    int player_mode = atoi(player_mode_str);
    if (player_mode != 1 && player_mode != 2) {
        player_mode = 1; // Default to Singleplayer
    }
    config->max_players = (player_mode == 1) ? 1 : MAX_PLAYERS;
    start_y += 5;
    
    // Port
    attron(A_BOLD);
    mvprintw(start_y, start_x, "Server port:");
    attroff(A_BOLD);
    mvprintw(start_y, start_x + 13, "[default: 8888] ");
    move(start_y, start_x + 29);
    refresh();
    char port_str[10];
    getnstr(port_str, 9);
    flushinp();  // Clear input buffer
    *port = atoi(port_str);
    if (*port <= 0) *port = DEFAULT_PORT;
    
    noecho();
    nodelay(stdscr, TRUE);
    
    return true;
}

bool get_connection_info(int *port, char *player_name) {
    clear();
    nodelay(stdscr, FALSE);
    echo();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_y = max_y / 2 - 5;
    int start_x = max_x / 2 - 25;
    
    // Title
    attron(A_BOLD | A_UNDERLINE);
    mvprintw(start_y, start_x + 10, "=== JOIN GAME ===");
    attroff(A_BOLD | A_UNDERLINE);
    start_y += 3;
    
    // Player name
    attron(A_BOLD);
    mvprintw(start_y, start_x, "Your name:");
    attroff(A_BOLD);
    mvprintw(start_y, start_x + 11, " ");
    move(start_y, start_x + 12);
    refresh();
    getnstr(player_name, MAX_NAME_LENGTH - 1);
    flushinp();  // Clear input buffer
    start_y += 2;
    
    // Server port
    attron(A_BOLD);
    mvprintw(start_y, start_x, "Server port:");
    attroff(A_BOLD);
    mvprintw(start_y, start_x + 13, "[default: 8888] ");
    move(start_y, start_x + 29);
    refresh();
    char port_str[10];
    getnstr(port_str, 9);
    flushinp();  // Clear input buffer
    *port = atoi(port_str);
    if (*port <= 0) *port = DEFAULT_PORT;
    
    noecho();
    nodelay(stdscr, TRUE);
    
    return strlen(player_name) > 0;
}

void show_error(const char *message) {
    clear();
    nodelay(stdscr, FALSE);
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int msg_len = strlen(message);
    int box_width = msg_len + 6;
    if (box_width < 30) box_width = 30;
    
    int start_x = (max_x - box_width) / 2;
    int start_y = max_y / 2 - 2;
    
    // Draw box
    attron(A_BOLD | COLOR_PAIR(COLOR_PAIR_FOOD));
    for (int i = 0; i < box_width; i++) {
        mvaddch(start_y, start_x + i, ACS_HLINE);
        mvaddch(start_y + 4, start_x + i, ACS_HLINE);
    }
    for (int i = 1; i < 4; i++) {
        mvaddch(start_y + i, start_x, ACS_VLINE);
        mvaddch(start_y + i, start_x + box_width - 1, ACS_VLINE);
    }
    mvaddch(start_y, start_x, ACS_ULCORNER);
    mvaddch(start_y, start_x + box_width - 1, ACS_URCORNER);
    mvaddch(start_y + 4, start_x, ACS_LLCORNER);
    mvaddch(start_y + 4, start_x + box_width - 1, ACS_LRCORNER);
    
    // Error message
    mvprintw(start_y + 2, (max_x - msg_len) / 2, "%s", message);
    attroff(A_BOLD | COLOR_PAIR(COLOR_PAIR_FOOD));
    
    // Press key prompt
    const char *prompt = "Press ENTER to continue...";
    mvprintw(start_y + 6, (max_x - strlen(prompt)) / 2, "%s", prompt);
    refresh();
    
    // Wait for Enter key only
    int key;
    do {
        key = getch();
    } while (key != '\n' && key != '\r' && key != KEY_ENTER);
    
    nodelay(stdscr, TRUE);
}

void show_game_over_stats(const GameState *state) {
    clear();
    nodelay(stdscr, FALSE);
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_y = max_y / 2 - 10;
    int start_x = max_x / 2 - 25;
    
    // Title with box
    attron(A_BOLD | A_REVERSE);
    mvprintw(start_y, start_x + 8, "                          ");
    mvprintw(start_y + 1, start_x + 8, "       GAME OVER         ");
    mvprintw(start_y + 2, start_x + 8, "                          ");
    attroff(A_BOLD | A_REVERSE);
    start_y += 5;
    
    // Sort players by score
    int sorted_ids[MAX_PLAYERS];
    int count = 0;
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->snakes[i].score > 0 || state->snakes[i].alive) {
            sorted_ids[count++] = i;
        }
    }
    
    // Bubble sort
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (state->snakes[sorted_ids[j]].score < state->snakes[sorted_ids[j + 1]].score) {
                int temp = sorted_ids[j];
                sorted_ids[j] = sorted_ids[j + 1];
                sorted_ids[j + 1] = temp;
            }
        }
    }
    
    // Final Scores header
    attron(A_BOLD | A_UNDERLINE);
    mvprintw(start_y, start_x, "FINAL SCORES:");
    attroff(A_BOLD | A_UNDERLINE);
    start_y += 2;
    
    // Draw scores
    for (int i = 0; i < count; i++) {
        int id = sorted_ids[i];
        
        if (i < 3) {
            attron(A_BOLD);
        }
        
        mvprintw(start_y++, start_x, "%d. %-15s %4d points", 
                i + 1, state->snakes[id].name, state->snakes[id].score);
        
        if (i < 3) {
            attroff(A_BOLD);
        }
    }
    
    // Game stats
    start_y += 2;
    attron(A_BOLD);
    mvprintw(start_y, start_x, "Game Statistics:");
    attroff(A_BOLD);
    start_y++;
    mvprintw(start_y, start_x + 2, "Total time: %d:%02d", 
            state->elapsed_time / 60, state->elapsed_time % 60);
    start_y++;
    mvprintw(start_y, start_x + 2, "Players: %d", count);
    
    // Press key prompt
    start_y += 3;
    const char *prompt = "Press ENTER to continue...";
    mvprintw(start_y, (max_x - strlen(prompt)) / 2, "%s", prompt);
    refresh();
    
    // Wait for Enter key only
    int key;
    do {
        key = getch();
    } while (key != '\n' && key != '\r' && key != KEY_ENTER);
    
    nodelay(stdscr, TRUE);
}
