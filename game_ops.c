#include <stdio.h>
#include <stdlib.h>
#include "game_entities.h"
#define BOARD_SIZE 10

/*Initializes the board with the NULLs in each cell*/
void initialize_board(Battleship_cell*** board) {
    board = malloc(BOARD_SIZE * sizeof(Battleship_cell*));
    for (int i = 0; i < BOARD_SIZE; i++) {
        board[i] = malloc(BOARD_SIZE * sizeof(Battleship_cell));
    }
}

/*Returns 0 if the battleship could be inserted succesfully and 1 if it could not be inserted at the defined location*/
int check_insertion(Battleship_cell*** board, Battleship* battleship, int x, int y) {
    for (int i = 0; i < battleship->width; i++) {
        if (y - 1 >= 0 && x + i < BOARD_SIZE) {
            if (board[y - 1][x + i] != NULL) {
                return 1;
            }
        }
        if (y + battleship->height + 1 < BOARD_SIZE && x + i < BOARD_SIZE) {
            if (board[y + battleship->height + 1][x + i] != NULL) {
                return 1;
            }
        }
        if (x + i >= BOARD_SIZE)
            return 1;
    }

    for (int i = 0; i < battleship->height; i++) {
        if (y + i < BOARD_SIZE && x - 1 >= 0) {
            if (board[y + i][x - 1] != NULL) {
                return 1;
            }
        }
        if (y + i < BOARD_SIZE && x + 1 >= BOARD_SIZE) {
            if (board[y + i][x + 1] != NULL) {
                return 1;
            }
        }
        if (y + i >= BOARD_SIZE)
            return 1;
    }

    for (int i = 0; i < battleship->width; i++) {
        for (int j = 0; j < battleship->height; j++) {
            if (board[y + j][x + i] != NULL)
                return 1;
        }
    }
    return 0;
}

/*Returns 0 if the battleship was inserted succesfully and 1 if it could not be inserted at the defined location*/
int insert_battleship(Battleship_cell*** board, Battleship* battleship,
    int battleship_orientation, int x, int y) {
    if (battleship_orientation == 1) {
        int buf = battleship->width;
        battleship->width = battleship->height;
        battleship->height = buf;
    }
    int result = check_insertion(board, battleship, x, y);
    if (result == 1)
        return 1;
    for (int i = 0; i < battleship->width; i++) {
        for (int j = 0; j < battleship->height; j++) {
            board[y + j][x + i] = malloc(sizeof(Battleship_cell));
            board[y + j][x + i]->battleship_id = battleship->id;
        }
    }
}

/*Returns 0 if the battleship cell is damaged as the result of the hit and 1 otherwise*/
int hit_battleship(Battleship_cell*** board, int x, int y) {
    if (board[y][x] == NULL)
        return 0;
    if (board[y][x]->hit == 1)
        return 0;
    board[y][x]->hit = 1;
    return 1;
}

/*Free the memory allocated by the board*/
int free_board(Battleship_cell*** board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        free(board[i]);
    }
    free(board);
}
