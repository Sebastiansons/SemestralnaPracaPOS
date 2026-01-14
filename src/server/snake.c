#include "snake.h"
#include <string.h>
#include <stdlib.h>

void init_snake(Snake *snake, int player_id, int start_x, int start_y, const char *name) {//inicializuje hada na startovnej pozicii
    snake->player_id = player_id;//nastav ID hraca
    snake->length = 3;//zaciatocna dlzka 3 segmenty
    snake->direction = DIR_RIGHT;//zaciatocny smer doprava
    snake->pending_direction = DIR_NONE;//ziadny cakajuci smer
    snake->score = 0;//zaciatocne skore 0
    snake->alive = true;//had je zivy
    snake->paused = false;//had nie je pozastaveny
    snake->spawn_time = 0;//cas spawnu nastavi game logic
    strncpy(snake->name, name, MAX_NAME_LENGTH - 1);//skopiruj meno hraca
    snake->name[MAX_NAME_LENGTH - 1] = '\0';//ukoncovaci znak
    
    // Initialize snake body (horizontal)
    for (int i = 0; i < snake->length; i++) {//vytvor horizontalne telo hada
        snake->positions[i].x = start_x - i;//x suradnica (iduce segmenty vlavo)
        snake->positions[i].y = start_y;//y suradnica (rovnaka pre vsetky)
    }
}

void move_snake(Snake *snake, int width, int height, bool wrap_around) {//posunie hada o jeden krok v aktualnom smere
    if (!snake->alive || snake->paused || snake->direction == DIR_NONE) {//ak je mrtvy, pozastaveny alebo bez smeru
        return;//nehybaj sa
    }
    
    // Apply pending direction if there is one
    if (snake->pending_direction != DIR_NONE) {//ak je cakajuci smer
        snake->direction = snake->pending_direction;//aplikuj ho
        snake->pending_direction = DIR_NONE;//vymaz cakajuci smer
    }
    
    // Calculate new head position
    Position new_head = snake->positions[0];//nova pozicia hlavy (zatial stara)
    
    switch (snake->direction) {//vypocitaj novu poziciu podla smeru
        case DIR_UP:
            new_head.y--;//hore
            break;
        case DIR_DOWN:
            new_head.y++;//dole
            break;
        case DIR_LEFT:
            new_head.x--;//vlavo
            break;
        case DIR_RIGHT:
            new_head.x++;//vpravo
            break;
        case DIR_NONE:
            return;//ziadny smer
    }
    
    // Handle wrapping
    if (wrap_around) {//ako sa ma mapa chovat(mode bez prekazok)
        if (new_head.x < 0) new_head.x = width - 1;//presiel cez lavu hranu -> objavil sa vpravo
        if (new_head.x >= width) new_head.x = 0;//presiel cez pravu hranu -> objavil sa vlavo
        if (new_head.y < 0) new_head.y = height - 1;//presiel cez hornu hranu -> objavil sa dole
        if (new_head.y >= height) new_head.y = 0;//presiel cez dolnu hranu -> objavil sa hore
    }
    
    // Move body
    for (int i = snake->length - 1; i > 0; i--) {//posun cely telo hada
        snake->positions[i] = snake->positions[i - 1];//kazdy segment sa posunie na poziciu predchadzajuceho
    }
    
    snake->positions[0] = new_head;//nastav novu poziciu hlavy
}

bool check_self_collision(const Snake *snake) {//skontroluje ci had narazil sam do seba
    if (!snake->alive) {//ak je mrtvy
        return false;//nema zmysel kontrolovat
    }
    
    Position head = snake->positions[0];//pozicia hlavy
    
    // Check if head collides with body (skip first segment)
    for (int i = 1; i < snake->length; i++) {//prejdi vsetky segmenty tela (preskoc hlavu)
        if (head.x == snake->positions[i].x && head.y == snake->positions[i].y) {//ak hlava ma rovnaku poziciu ako nejaky segment
            return true;//kolizia
        }
    }
    
    return false;//ziadna kolizia
}

bool check_collision_with_snake(const Snake *snake1, const Snake *snake2) {//skontroluje ci had1 narazil do hada2
    if (!snake1->alive || !snake2->alive) {//ak je niektory mrtvy
        return false;//nema zmysel kontrolovat
    }
    
    Position head1 = snake1->positions[0];//pozicia hlavy hada1
    
    // Check if snake1's head collides with snake2's BODY (not head)
    // Start from i=1 to skip snake2's head (only check body segments)
    for (int i = 1; i < snake2->length; i++) {//prejdi telo hada2 (preskoc hlavu)
        if (head1.x == snake2->positions[i].x && head1.y == snake2->positions[i].y) {//ak hlava hada1 ma rovnaku poziciu ako segment hada2
            return true;//kolizia
        }
    }
    
    return false;//ziadna kolizia
}

void grow_snake(Snake *snake) {//zvacsi hada o jeden segment
    if (snake->length < MAX_SNAKE_LENGTH) {//ak este nie je na maximalnej dlzke
        // Add new segment at the tail
        snake->positions[snake->length] = snake->positions[snake->length - 1];//pridaj segment na koniec (duplikuj chvost)
        snake->length++;//zvys dlzku
        snake->score += 10;//pridaj skore
    }
}

bool is_position_on_snake(const Snake *snake, Position pos) {//skontroluje ci je pozicia na hadovi
    if (!snake->alive) {//ak je had mrtvy
        return false;//nema zmysel kontrolovat
    }
    
    for (int i = 0; i < snake->length; i++) {//prejdi vsetky segmenty hada
        if (snake->positions[i].x == pos.x && snake->positions[i].y == pos.y) {//ak sa pozicia zhoduje
            return true;//pozicia je na hadovi
        }
    }
    
    return false;//pozicia nie je na hadovi
}

void change_direction(Snake *snake, Direction new_direction) {//zmeni smer pohybu hada
    // Prevent 180 degree turns
    if (new_direction == DIR_NONE) {//ak je novy smer NONE
        return;//ignoruj
    }
    
    // Check against current direction (not pending)
    if ((snake->direction == DIR_UP && new_direction == DIR_DOWN) ||//zabran otoceniu o 180 stupnov
        (snake->direction == DIR_DOWN && new_direction == DIR_UP) ||//hore -> dole
        (snake->direction == DIR_LEFT && new_direction == DIR_RIGHT) ||//vlavo -> vpravo
        (snake->direction == DIR_RIGHT && new_direction == DIR_LEFT)) {//vpravo -> vlavo
        return;//ignoruj
    }
    
    // Set as pending direction to be applied on next move
    // This prevents multiple direction changes between ticks
    snake->pending_direction = new_direction;//nastav cakajuci smer (aplikuje sa na dalsom tiku)
}
