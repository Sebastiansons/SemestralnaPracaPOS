#include "ui.h"
#include <string.h>
#include <stdlib.h>

#define COLOR_PAIR_SNAKE_BASE 1
#define COLOR_PAIR_FOOD 9
#define COLOR_PAIR_OBSTACLE 10

bool init_ui(void) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    
    if (has_colors()) {
        start_color();
        
        // Snake colors (different for each player)
        init_pair(1, COLOR_GREEN, COLOR_BLACK);
        init_pair(2, COLOR_BLUE, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
        init_pair(4, COLOR_YELLOW, COLOR_BLACK);
        init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
        init_pair(6, COLOR_CYAN, COLOR_BLACK);
        init_pair(7, COLOR_WHITE, COLOR_BLACK);
        init_pair(8, COLOR_GREEN, COLOR_BLACK);
        
        // Food and obstacles
        init_pair(COLOR_PAIR_FOOD, COLOR_RED, COLOR_BLACK);
        init_pair(COLOR_PAIR_OBSTACLE, COLOR_WHITE, COLOR_BLACK);
    }
    
    return true;
}

void cleanup_ui(void) {
    endwin();
}

int get_color_pair(int player_id) {
    return COLOR_PAIR_SNAKE_BASE + (player_id % 8);
}

void render_game_state(const GameState *state, int my_player_id) {
    clear();
    
    int start_y = 2;
    int start_x = 2;
    
    // Draw border
    for (int y = 0; y < state->height + 2; y++) {
        mvaddch(start_y + y, start_x - 1, ACS_VLINE);
        mvaddch(start_y + y, start_x + state->width, ACS_VLINE);
    }
    for (int x = 0; x < state->width + 2; x++) {
        mvaddch(start_y - 1, start_x + x - 1, ACS_HLINE);
        mvaddch(start_y + state->height, start_x + x - 1, ACS_HLINE);
    }
    mvaddch(start_y - 1, start_x - 1, ACS_ULCORNER);
    mvaddch(start_y - 1, start_x + state->width, ACS_URCORNER);
    mvaddch(start_y + state->height, start_x - 1, ACS_LLCORNER);
    mvaddch(start_y + state->height, start_x + state->width, ACS_LRCORNER);
    
    // Draw obstacles
    if (state->obstacles) {
        attron(COLOR_PAIR(COLOR_PAIR_OBSTACLE));
        for (int y = 0; y < state->height; y++) {
            for (int x = 0; x < state->width; x++) {
                if (state->obstacles[y * state->width + x] != 0) {
                    mvaddch(start_y + y, start_x + x, '#');
                }
            }
        }
        attroff(COLOR_PAIR(COLOR_PAIR_OBSTACLE));
    }
    
    // Draw food
    attron(COLOR_PAIR(COLOR_PAIR_FOOD) | A_BOLD);
    for (int i = 0; i < state->food_count; i++) {
        mvaddch(start_y + state->food[i].y, start_x + state->food[i].x, '*');
    }
    attroff(COLOR_PAIR(COLOR_PAIR_FOOD) | A_BOLD);
    
    // Draw snakes
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->snakes[i].alive) {
            int color = get_color_pair(i);
            attron(COLOR_PAIR(color));
            
            // Draw head
            if (state->snakes[i].length > 0) {
                int attr = A_BOLD;
                if (i == my_player_id) {
                    attr |= A_REVERSE;
                }
                attron(attr);
                mvaddch(start_y + state->snakes[i].positions[0].y, 
                       start_x + state->snakes[i].positions[0].x, 'O');
                attroff(attr);
                
                // Draw body
                for (int j = 1; j < state->snakes[i].length; j++) {
                    mvaddch(start_y + state->snakes[i].positions[j].y, 
                           start_x + state->snakes[i].positions[j].x, 'o');
                }
            }
            
            attroff(COLOR_PAIR(color));
        }
    }
    
    // Draw info panel
    int info_x = start_x + state->width + 5;
    int info_y = start_y;
    
    mvprintw(0, start_x, "Snake Game - Press 'q' to quit, 'p' to pause");
    
    mvprintw(info_y++, info_x, "Time: %d:%02d", state->elapsed_time / 60, state->elapsed_time % 60);
    if (state->mode == MODE_TIMED && state->time_limit > 0) {
        int remaining = state->time_limit - state->elapsed_time;
        mvprintw(info_y++, info_x, "Remaining: %d:%02d", remaining / 60, remaining % 60);
    }
    info_y++;
    
    mvprintw(info_y++, info_x, "Players:");
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (state->snakes[i].alive) {
            int color = get_color_pair(i);
            attron(COLOR_PAIR(color));
            
            char status[32] = "";
            if (state->snakes[i].paused) {
                strcpy(status, " [PAUSED]");
            }
            if (i == my_player_id) {
                mvprintw(info_y++, info_x, "> %s: %d%s", 
                        state->snakes[i].name, state->snakes[i].score, status);
            } else {
                mvprintw(info_y++, info_x, "  %s: %d%s", 
                        state->snakes[i].name, state->snakes[i].score, status);
            }
            
            attroff(COLOR_PAIR(color));
        }
    }
    
    if (state->game_over) {
        int center_y = start_y + state->height / 2;
        int center_x = start_x + state->width / 2;
        attron(A_BOLD | A_REVERSE);
        mvprintw(center_y, center_x - 5, " GAME OVER ");
        attroff(A_BOLD | A_REVERSE);
    }
    
    refresh();
}

void render_message(const char *message) {
    clear();
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    mvprintw(max_y / 2, (max_x - strlen(message)) / 2, "%s", message);
    refresh();
}

void clear_screen(void) {
    clear();
    refresh();
}
