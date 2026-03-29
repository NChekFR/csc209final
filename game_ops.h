#ifndef GAME_OPS_H
#define GAME_OPS_H

#include "game_entities.h"

#define BOARD_SIZE 10
#define BOARD_SERIALIZED_SIZE (BOARD_SIZE * (BOARD_SIZE + 1))

/*Initializes the board with the NULLs in each cell*/
Battleship_cell*** initialize_board(void);

/*Returns 0 if the battleship could be inserted succesfully and 1 if it could not be inserted at the defined location*/
int check_insertion(Battleship_cell*** board, int width, int height, int x, int y);

/*Returns 0 if the battleship was inserted succesfully and 1 if it could not be inserted at the defined location*/
int insert_battleship(Battleship_cell*** board, Battleship* battleship,
    int battleship_orientation, int x, int y);

/*Returns 0 if the battleship cell is damaged as the result of the hit and 1 otherwise.
 *Returns -1 if the error occured.*/
int hit_battleship(Battleship_cell*** board, int x, int y);

/*Free the memory allocated by the board*/
void free_board(Battleship_cell*** board);

char* return_board(Battleship_cell*** board, int is_opponent);

#endif