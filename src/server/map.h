#ifndef MAP_H
#define MAP_H

#include "protocol.h"
#include <stdbool.h>

// Map functions
bool load_map_from_file(const char *filename, uint8_t **obstacles, int *width, int *height);
void generate_random_map(uint8_t **obstacles, int width, int height, float obstacle_density);
bool is_obstacle(const uint8_t *obstacles, int x, int y, int width);
void set_obstacle(uint8_t *obstacles, int x, int y, int width);
bool is_reachable(const uint8_t *obstacles, int width, int height, Position start);
void free_obstacles(uint8_t *obstacles);

#endif // MAP_H
