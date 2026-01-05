#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool load_map_from_file(const char *filename, uint8_t **obstacles, int *width, int *height) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        return false;
    }
    
    // Read dimensions
    if (fscanf(file, "%d %d\n", width, height) != 2) {
        fclose(file);
        return false;
    }
    
    // Allocate obstacle map
    *obstacles = (uint8_t *)calloc((*width) * (*height), sizeof(uint8_t));
    if (!*obstacles) {
        fclose(file);
        return false;
    }
    
    // Read map
    char line[1024];
    int y = 0;
    while (fgets(line, sizeof(line), file) && y < *height) {
        for (int x = 0; x < *width && line[x] != '\0' && line[x] != '\n'; x++) {
            if (line[x] == '#') {
                set_obstacle(*obstacles, x, y, *width);
            }
        }
        y++;
    }
    
    fclose(file);
    return true;
}

void generate_random_map(uint8_t **obstacles, int width, int height, float obstacle_density) {
    *obstacles = (uint8_t *)calloc(width * height, sizeof(uint8_t));
    if (!*obstacles) {
        return;
    }
    
    srand(time(NULL));
    
    // Generate random obstacles
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            if ((float)rand() / RAND_MAX < obstacle_density) {
                set_obstacle(*obstacles, x, y, width);
            }
        }
    }
    
    // Ensure at least some free space in center
    Position center = {width / 2, height / 2};
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int x = center.x + dx;
            int y = center.y + dy;
            if (x >= 0 && x < width && y >= 0 && y < height) {
                (*obstacles)[y * width + x] = 0;
            }
        }
    }
}

bool is_obstacle(const uint8_t *obstacles, int x, int y, int width) {
    return obstacles[y * width + x] != 0;
}

void set_obstacle(uint8_t *obstacles, int x, int y, int width) {
    obstacles[y * width + x] = 1;
}

// BFS to check if all free cells are reachable
bool is_reachable(const uint8_t *obstacles, int width, int height, Position start) {
    if (obstacles[start.y * width + start.x] != 0) {
        return false;
    }
    
    uint8_t *visited = (uint8_t *)calloc(width * height, sizeof(uint8_t));
    if (!visited) {
        return false;
    }
    
    // Simple BFS
    Position *queue = (Position *)malloc(width * height * sizeof(Position));
    if (!queue) {
        free(visited);
        return false;
    }
    
    int front = 0, back = 0;
    queue[back++] = start;
    visited[start.y * width + start.x] = 1;
    
    int free_cells = 0;
    int reachable_cells = 0;
    
    // Count total free cells
    for (int i = 0; i < width * height; i++) {
        if (obstacles[i] == 0) {
            free_cells++;
        }
    }
    
    while (front < back) {
        Position current = queue[front++];
        reachable_cells++;
        
        // Check 4 directions
        int dx[] = {0, 0, -1, 1};
        int dy[] = {-1, 1, 0, 0};
        
        for (int i = 0; i < 4; i++) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {
                int idx = ny * width + nx;
                if (obstacles[idx] == 0 && visited[idx] == 0) {
                    visited[idx] = 1;
                    queue[back++] = (Position){nx, ny};
                }
            }
        }
    }
    
    free(queue);
    free(visited);
    
    return reachable_cells == free_cells;
}

void free_obstacles(uint8_t *obstacles) {
    if (obstacles) {
        free(obstacles);
    }
}
