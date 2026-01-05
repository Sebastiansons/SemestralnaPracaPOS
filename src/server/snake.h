#ifndef SNAKE_H
#define SNAKE_H

#include "protocol.h"

// Snake functions
void init_snake(Snake *snake, int player_id, int start_x, int start_y, const char *name);
void move_snake(Snake *snake, int width, int height, bool wrap_around);
bool check_self_collision(const Snake *snake);
bool check_collision_with_snake(const Snake *snake1, const Snake *snake2);
void grow_snake(Snake *snake);
bool is_position_on_snake(const Snake *snake, Position pos);
void change_direction(Snake *snake, Direction new_direction);

#endif // SNAKE_H
