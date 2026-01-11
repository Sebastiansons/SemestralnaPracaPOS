/**
 * @file snake.h
 * @brief Snake movement and collision detection
 * 
 * Provides functions for snake initialization, movement, growth,
 * collision detection, and direction changes.
 */

#ifndef SNAKE_H
#define SNAKE_H

#include "protocol.h"

/**
 * @brief Initialize snake at starting position
 * @param snake Snake to initialize
 * @param player_id Player ID (0-7)
 * @param start_x Starting X coordinate
 * @param start_y Starting Y coordinate
 * @param name Player name
 */
void init_snake(Snake *snake, int player_id, int start_x, int start_y, const char *name);

/**
 * @brief Move snake one step in current direction
 * @param snake Snake to move
 * @param width World width
 * @param height World height
 * @param wrap_around Enable wrapping around edges (no obstacles mode)
 * 
 * Moves head forward, removes tail. Does not move if paused or direction is NONE.
 */
void move_snake(Snake *snake, int width, int height, bool wrap_around);

/**
 * @brief Check if snake collides with itself
 * @param snake Snake to check
 * @return true if head collides with body, false otherwise
 */
bool check_self_collision(const Snake *snake);

/**
 * @brief Check collision between two snakes
 * @param snake1 First snake
 * @param snake2 Second snake
 * @return true if snake1's head collides with snake2's body, false otherwise
 */
bool check_collision_with_snake(const Snake *snake1, const Snake *snake2);

/**
 * @brief Grow snake by one segment
 * @param snake Snake to grow
 * 
 * Duplicates tail position, effectively extending snake by 1.
 */
void grow_snake(Snake *snake);

/**
 * @brief Check if position is on snake's body
 * @param snake Snake to check
 * @param pos Position to check
 * @return true if position is on snake, false otherwise
 */
bool is_position_on_snake(const Snake *snake, Position pos);

/**
 * @brief Change snake's direction
 * @param snake Snake to change
 * @param new_direction New direction
 * 
 * Prevents 180-degree turns (opposite direction).
 * Stores new direction in pending_direction for next tick.
 */
void change_direction(Snake *snake, Direction new_direction);

#endif // SNAKE_H
