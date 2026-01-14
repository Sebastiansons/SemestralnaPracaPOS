#include "map.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

bool load_map_from_file(const char *filename, uint8_t **obstacles, int *width, int *height) {//nacita mapu zo suboru
    FILE *file = fopen(filename, "r");//otvor subor na citanie
    if (!file) {//ak sa nepodarilo otvorit
        return false;
    }
    
    // Read dimensions
    if (fscanf(file, "%d %d\n", width, height) != 2) {//precitaj rozmery (sirka vyska)
        fclose(file);
        return false;
    }
    
    // Allocate obstacle map
    *obstacles = (uint8_t *)calloc((*width) * (*height), sizeof(uint8_t));//alokuj pamat pre bitmapu prekazok
    if (!*obstacles) {//ak sa nepodarilo alokovat
        fclose(file);
        return false;
    }
    
    // Read map
    char line[1024];//buffer pre riadok
    int y = 0;//aktualny riadok
    while (fgets(line, sizeof(line), file) && y < *height) {//citaj po riadkoch
        for (int x = 0; x < *width && line[x] != '\0' && line[x] != '\n'; x++) {//prejdi znaky v riadku
            if (line[x] == '#') {//ak je to znak prekazky
                set_obstacle(*obstacles, x, y, *width);//nastav prekazku na tejto pozicii
            }
        }
        y++;
    }
    
    fclose(file);//zatvor subor
    return true;//nacitanie uspesne
}

void generate_random_map(uint8_t **obstacles, int width, int height, float obstacle_density) {//vygeneruje nahodnu mapu s prekazkami
    *obstacles = (uint8_t *)calloc(width * height, sizeof(uint8_t));//alokuj pamat pre bitmapu prekazok
    if (!*obstacles) {//ak sa nepodarilo alokovat
        return;
    }
    
    srand(time(NULL));//inicializuj generator nahodnych cisel
    
    Position center = {width / 2, height / 2};//stred mapy
    
    // Define safe spawn zone radius (min 10 units from center)
    int safe_radius = (width < height ? width : height) / 4;//bezpecna zona okolo stredu (1/4 mensej dimenzie)
    if (safe_radius < 10) {//minimum 10 buniek
        safe_radius = 10;
    }
    
    // Generate random obstacles (but not too close to center)
    for (int y = 0; y < height; y++) {//prejdi vsetky riadky
        for (int x = 0; x < width; x++) {//prejdi vsetky stlpce
            // Calculate distance from center
            int dx = x - center.x;//rozdiel x
            int dy = y - center.y;//rozdiel y
            int dist_sq = dx * dx + dy * dy;//stvorec vzdialenosti od stredu
            
            // Only place obstacles outside safe radius
            if (dist_sq > safe_radius * safe_radius) {//ak je mimo bezpecnej zony
                if ((float)rand() / RAND_MAX < obstacle_density) {//s pravdepodobnostou podla hustoty
                    set_obstacle(*obstacles, x, y, width);//nastav prekazku
                }
            }
        }
    }
}

bool is_obstacle(const uint8_t *obstacles, int x, int y, int width) {//skontroluje ci je na pozicii prekazka
    return obstacles[y * width + x] != 0;//vrat true ak je prekazka
}

void set_obstacle(uint8_t *obstacles, int x, int y, int width) {//nastavi prekazku na poziciu
    obstacles[y * width + x] = 1;//oznac poziciu ako prekazku
}

// BFS to check if all free cells are reachable
bool is_reachable(const uint8_t *obstacles, int width, int height, Position start) {//BFS pre overenie ci su vsetky volne bunky dosiahnute
    if (obstacles[start.y * width + start.x] != 0) {//ak je startova pozicia prekazka
        return false;
    }
    
    uint8_t *visited = (uint8_t *)calloc(width * height, sizeof(uint8_t));//pole navstivenych buniek
    if (!visited) {
        return false;
    }
    
    // Simple BFS
    Position *queue = (Position *)malloc(width * height * sizeof(Position));//fronta pre BFS
    if (!queue) {
        free(visited);
        return false;
    }
    
    int front = 0, back = 0;//indexy fronty
    queue[back++] = start;//pridaj startovu poziciu do fronty
    visited[start.y * width + start.x] = 1;//oznac ju ako navstivenu
    
    int free_cells = 0;//pocet volnych buniek
    int reachable_cells = 0;//pocet dosiahnutych buniek
    
    // Count total free cells
    for (int i = 0; i < width * height; i++) {//spocitaj vsetky volne bunky
        if (obstacles[i] == 0) {
            free_cells++;
        }
    }
    
    while (front < back) {//kym nie je fronta prazdna
        Position current = queue[front++];//vezmi poziciu z fronty
        reachable_cells++;//pripocitaj dosiahnute bunky
        
        // Check 4 directions
        int dx[] = {0, 0, -1, 1};//posuny x (hore, dole, vlavo, vpravo)
        int dy[] = {-1, 1, 0, 0};//posuny y
        
        for (int i = 0; i < 4; i++) {//skontroluj vsetkych 4 susedov
            int nx = current.x + dx[i];//nova x suradnica
            int ny = current.y + dy[i];//nova y suradnica
            
            if (nx >= 0 && nx < width && ny >= 0 && ny < height) {//ak je v hraniciach mapy
                int idx = ny * width + nx;//index v bitove mape
                if (obstacles[idx] == 0 && visited[idx] == 0) {//ak nie je prekazka a nebola navstivena
                    visited[idx] = 1;//oznac ako navstivenu
                    queue[back++] = (Position){nx, ny};//pridaj do fronty
                }
            }
        }
    }
    
    free(queue);//uvolni pamat fronty
    free(visited);//uvolni pamat navstivenych
    
    return reachable_cells == free_cells;//vrat true ak su vsetky volne bunky dosiahnute
}

void free_obstacles(uint8_t *obstacles) {//uvolni pamat bitmapy prekazok
    if (obstacles) {//ak existuje
        free(obstacles);//uvolni pamat
    }
}
