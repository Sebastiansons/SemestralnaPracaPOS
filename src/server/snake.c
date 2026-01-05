#include "snake.h"
#include <string.h>
#include <stdlib.h>

void init_snake(Snake *snake, int player_id, int start_x, int start_y, const char *name) {
    snake->player_id = player_id;
    snake->length = 3;
    snake->direction = DIR_RIGHT;
    snake->score = 0;
    snake->alive = true;
    snake->paused = false;
    strncpy(snake->name, name, MAX_NAME_LENGTH - 1);
    snake->name[MAX_NAME_LENGTH - 1] = '\0';
    
    // Initialize snake body (horizontal)
    for (int i = 0; i < snake->length; i++) {
        snake->positions[i].x = start_x - i;
        snake->positions[i].y = start_y;
    }
}

void move_snake(Snake *snake, int width, int height, bool wrap_around) {
    if (!snake->alive || snake->paused || snake->direction == DIR_NONE) {
        return;
    }
    
    // Calculate new head position
    Position new_head = snake->positions[0];
    
    switch (snake->direction) {
        case DIR_UP:
            new_head.y--;
            break;
        case DIR_DOWN:
            new_head.y++;
            break;
        case DIR_LEFT:
            new_head.x--;
            break;
        case DIR_RIGHT:
            new_head.x++;
            break;
        case DIR_NONE:
            return;
    }
    
    // Handle wrapping
    if (wrap_around) {
        if (new_head.x < 0) new_head.x = width - 1;
        if (new_head.x >= width) new_head.x = 0;
        if (new_head.y < 0) new_head.y = height - 1;
        if (new_head.y >= height) new_head.y = 0;
    }
    
    // Move body
    for (int i = snake->length - 1; i > 0; i--) {
        snake->positions[i] = snake->positions[i - 1];
    }
    
    snake->positions[0] = new_head;
}

bool check_self_collision(const Snake *snake) {
    if (!snake->alive) {
        return false;
    }
    
    Position head = snake->positions[0];
    
    // Check if head collides with body (skip first segment)
    for (int i = 1; i < snake->length; i++) {
        if (head.x == snake->positions[i].x && head.y == snake->positions[i].y) {
            return true;
        }
    }
    
    return false;
}

bool check_collision_with_snake(const Snake *snake1, const Snake *snake2) {
    if (!snake1->alive || !snake2->alive) {
        return false;
    }
    
    Position head1 = snake1->positions[0];
    
    // Check if snake1's head collides with snake2's body
    for (int i = 0; i < snake2->length; i++) {
        if (head1.x == snake2->positions[i].x && head1.y == snake2->positions[i].y) {
            return true;
        }
    }
    
    return false;
}

void grow_snake(Snake *snake) {
    if (snake->length < MAX_SNAKE_LENGTH) {
        // Add new segment at the tail
        snake->positions[snake->length] = snake->positions[snake->length - 1];
        snake->length++;
        snake->score += 10;
    }
}

bool is_position_on_snake(const Snake *snake, Position pos) {
    if (!snake->alive) {
        return false;
    }
    
    for (int i = 0; i < snake->length; i++) {
        if (snake->positions[i].x == pos.x && snake->positions[i].y == pos.y) {
            return true;
        }
    }
    
    return false;
}

void change_direction(Snake *snake, Direction new_direction) {
    // Prevent 180 degree turns
    if (new_direction == DIR_NONE) {
        return;
    }
    
    if ((snake->direction == DIR_UP && new_direction == DIR_DOWN) ||
        (snake->direction == DIR_DOWN && new_direction == DIR_UP) ||
        (snake->direction == DIR_LEFT && new_direction == DIR_RIGHT) ||
        (snake->direction == DIR_RIGHT && new_direction == DIR_LEFT)) {
        return;
    }
    
    snake->direction = new_direction;
}
