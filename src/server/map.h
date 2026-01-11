/**
 * @file map.h
 * @brief Map loading, generation, and validation
 * 
 * Handles obstacle map loading from files, random generation with safe spawn zones,
 * reachability verification using BFS, and obstacle manipulation.
 */

#ifndef MAP_H
#define MAP_H

#include "protocol.h"
#include <stdbool.h>

/**
 * @brief Load obstacle map from text file
 * @param filename File path
 * @param obstacles Output obstacle bitmap (allocated by function)
 * @param width Output map width
 * @param height Output map height
 * @return true on success, false on failure
 * 
 * File format: '#' = obstacle, '.' or ' ' = empty
 */
bool load_map_from_file(const char *filename, uint8_t **obstacles, int *width, int *height);

/**
 * @brief Generate random obstacle map with safe spawn zone
 * @param obstacles Output obstacle bitmap (allocated by function)
 * @param width Map width
 * @param height Map height
 * @param obstacle_density Obstacle density (0.0-1.0, e.g., 0.10 = 10%)
 * 
 * Generates obstacles only outside safe radius from center.
 * Safe radius = width/4 or height/4 (minimum 10).
 */
void generate_random_map(uint8_t **obstacles, int width, int height, float obstacle_density);

/**
 * @brief Check if position has obstacle
 * @param obstacles Obstacle bitmap
 * @param x X coordinate
 * @param y Y coordinate
 * @param width Map width
 * @return true if obstacle at position, false otherwise
 */
bool is_obstacle(const uint8_t *obstacles, int x, int y, int width);

/**
 * @brief Set obstacle at position
 * @param obstacles Obstacle bitmap
 * @param x X coordinate
 * @param y Y coordinate
 * @param width Map width
 */
void set_obstacle(uint8_t *obstacles, int x, int y, int width);

/**
 * @brief Check if position is reachable from start using BFS
 * @param obstacles Obstacle bitmap
 * @param width Map width
 * @param height Map height
 * @param start Starting position
 * @return true if all non-obstacle cells are reachable, false otherwise
 * 
 * Uses breadth-first search to verify map connectivity.
 */
bool is_reachable(const uint8_t *obstacles, int width, int height, Position start);

/**
 * @brief Free obstacle bitmap memory
 * @param obstacles Obstacle bitmap to free
 */
void free_obstacles(uint8_t *obstacles);

#endif // MAP_H
