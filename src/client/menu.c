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
    const char *options[] = {
        "1. New Game",
        "2. Join Game",
        "3. Resume Game",
        "4. Exit"
    };
    int num_options = can_resume ? 4 : 3;
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    
    int start_y = max_y / 2 - 5;
    int start_x = max_x / 2 - strlen(title) / 2;
    
    attron(A_BOLD);
    mvprintw(start_y, start_x, "%s", title);
    attroff(A_BOLD);
    
    start_y += 2;
    for (int i = 0; i < num_options; i++) {
        if (i == 2 && !can_resume) continue;
        mvprintw(start_y + i, start_x, "%s", options[i]);
    }
    
    mvprintw(start_y + num_options + 2, start_x - 10, "Enter your choice: ");
    refresh();
    
    int choice = getch();
    
    nodelay(stdscr, TRUE);
    
    switch (choice) {
        case '1': return MENU_NEW_GAME;
        case '2': return MENU_JOIN_GAME;
        case '3': return can_resume ? MENU_RESUME_GAME : MENU_CANCEL;
        case '4': return MENU_EXIT;
        default: return MENU_CANCEL;
    }
}

bool get_game_config(GameConfig *config) {
    clear();
    nodelay(stdscr, FALSE);
    echo();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_y = max_y / 2 - 8;
    int start_x = 10;
    
    attron(A_BOLD);
    mvprintw(start_y++, start_x, "=== NEW GAME CONFIGURATION ===");
    attroff(A_BOLD);
    start_y += 2;
    
    // Mode
    mvprintw(start_y++, start_x, "Game mode (1=Standard, 2=Timed): ");
    char mode_str[10];
    getnstr(mode_str, 9);
    config->mode = (atoi(mode_str) == 2) ? MODE_TIMED : MODE_STANDARD;
    
    // Time limit
    if (config->mode == MODE_TIMED) {
        mvprintw(start_y++, start_x, "Time limit (seconds): ");
        char time_str[10];
        getnstr(time_str, 9);
        config->time_limit = atoi(time_str);
        if (config->time_limit <= 0) config->time_limit = 300;
    } else {
        config->time_limit = 0;
    }
    
    // World type
    mvprintw(start_y++, start_x, "World type (1=No obstacles, 2=With obstacles): ");
    char world_str[10];
    getnstr(world_str, 9);
    config->world_type = (atoi(world_str) == 2) ? WORLD_WITH_OBSTACLES : WORLD_NO_OBSTACLES;
    
    // Load from file
    config->load_from_file = false;
    if (config->world_type == WORLD_WITH_OBSTACLES) {
        mvprintw(start_y++, start_x, "Load map from file? (y/n): ");
        char load_str[10];
        getnstr(load_str, 9);
        
        if (load_str[0] == 'y' || load_str[0] == 'Y') {
            config->load_from_file = true;
            mvprintw(start_y++, start_x, "Map file path: ");
            getnstr(config->map_file, sizeof(config->map_file) - 1);
        }
    }
    
    // Dimensions
    if (!config->load_from_file) {
        mvprintw(start_y++, start_x, "Width (20-80): ");
        char width_str[10];
        getnstr(width_str, 9);
        config->width = atoi(width_str);
        if (config->width < 20) config->width = 40;
        if (config->width > 80) config->width = 80;
        
        mvprintw(start_y++, start_x, "Height (10-40): ");
        char height_str[10];
        getnstr(height_str, 9);
        config->height = atoi(height_str);
        if (config->height < 10) config->height = 20;
        if (config->height > 40) config->height = 40;
    } else {
        config->width = 40;
        config->height = 20;
    }
    
    // Port
    mvprintw(start_y++, start_x, "Server port (default 8888): ");
    char port_str[10];
    getnstr(port_str, 9);
    int port = atoi(port_str);
    if (port <= 0) port = DEFAULT_PORT;
    
    noecho();
    nodelay(stdscr, TRUE);
    
    return true;
}

bool get_connection_info(char *host, int *port, char *player_name) {
    clear();
    nodelay(stdscr, FALSE);
    echo();
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_y = max_y / 2 - 4;
    int start_x = 10;
    
    attron(A_BOLD);
    mvprintw(start_y++, start_x, "=== JOIN GAME ===");
    attroff(A_BOLD);
    start_y += 2;
    
    mvprintw(start_y++, start_x, "Your name: ");
    getnstr(player_name, MAX_NAME_LENGTH - 1);
    
    mvprintw(start_y++, start_x, "Server host (default localhost): ");
    char host_str[256];
    getnstr(host_str, 255);
    if (strlen(host_str) == 0) {
        strcpy(host, "127.0.0.1");
    } else {
        strcpy(host, host_str);
    }
    
    mvprintw(start_y++, start_x, "Server port (default 8888): ");
    char port_str[10];
    getnstr(port_str, 9);
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
    
    attron(A_BOLD | COLOR_PAIR(COLOR_PAIR_FOOD));
    mvprintw(max_y / 2, (max_x - strlen(message)) / 2, "%s", message);
    attroff(A_BOLD | COLOR_PAIR(COLOR_PAIR_FOOD));
    
    mvprintw(max_y / 2 + 2, (max_x - 20) / 2, "Press any key...");
    refresh();
    
    getch();
    nodelay(stdscr, TRUE);
}

void show_game_over_stats(const GameState *state) {
    clear();
    nodelay(stdscr, FALSE);
    
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    int start_y = max_y / 2 - 8;
    int start_x = max_x / 2 - 20;
    
    attron(A_BOLD);
    mvprintw(start_y++, start_x, "=== GAME OVER ===");
    attroff(A_BOLD);
    start_y += 2;
    
    mvprintw(start_y++, start_x, "Final Scores:");
    start_y++;
    
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
    
    for (int i = 0; i < count; i++) {
        int id = sorted_ids[i];
        mvprintw(start_y++, start_x, "%d. %s: %d points", 
                i + 1, state->snakes[id].name, state->snakes[id].score);
    }
    
    start_y += 2;
    mvprintw(start_y, start_x, "Total game time: %d:%02d", 
            state->elapsed_time / 60, state->elapsed_time % 60);
    
    start_y += 2;
    mvprintw(start_y, start_x, "Press any key...");
    refresh();
    
    getch();
    nodelay(stdscr, TRUE);
}
