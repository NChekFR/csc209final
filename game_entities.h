//
// Created by ilian on 17.03.2026.
//

#ifndef FINAL_PROJECT_GAME_ENTITIES_H
#define FINAL_PROJECT_GAME_ENTITIES_H
typedef struct {
    int battleship_id;
    int hit; //0 if well, 1 if is damaged
} Battleship_cell;

typedef struct {
    char name[20];
    int id;
    int width;
    int height;
} Battleship;
#endif //FINAL_PROJECT_GAME_ENTITIES_H