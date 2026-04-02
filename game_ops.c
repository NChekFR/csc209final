#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game_ops.h"


/*Initializes the board with the NULLs in each cell*/
Battleship_cell*** initialize_board(void) {
    Battleship_cell*** board = malloc(BOARD_SIZE * sizeof(Battleship_cell**));
    if (board == NULL) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < BOARD_SIZE; i++) {
        board[i] = calloc(BOARD_SIZE, sizeof(Battleship_cell*));
        if (board[i] == NULL) {
            perror("calloc");
            exit(1);
        }
    }

    return board;
}

/*Returns 0 if the battleship could be inserted succesfully and 1 if it could not be inserted at the defined location*/
int check_insertion(Battleship_cell*** board, int width, int height, int x, int y) {
    if (x < 0 || y < 0 || width <= 0 || height <= 0) {
        return 1;
    }

    if (x + width > BOARD_SIZE || y + height > BOARD_SIZE) {
        return 1;
    }

    for (int yy = y - 1; yy <= y + height; yy++) {
        for (int xx = x - 1; xx <= x + width; xx++) {
            if (yy >= 0 && yy < BOARD_SIZE && xx >= 0 && xx < BOARD_SIZE) {
                if (board[yy][xx] != NULL) {
                    return 1;
                }
            }
        }
    }

    return 0;
}

/*Returns 0 if the battleship was inserted succesfully and 1 if it could not be inserted at the defined location*/
int insert_battleship(Battleship_cell*** board, Battleship* battleship,
    int battleship_orientation, int x, int y) {
    int width = battleship->width;
    int height = battleship->height;
    if (battleship_orientation == 1) {
        int buf = width;
        width = height;
        height = buf;
    }
    else if (battleship_orientation != 0) {
        return 1;
    }

    int result = check_insertion(board, width, height, x, y);
    if (result == 1) {
        return 1;
    }

    for (int i = 0; i < width; i++) {
        for (int j = 0; j < height; j++) {
            board[y + j][x + i] = malloc(sizeof(Battleship_cell));
            if (board[y + j][x + i] == NULL) {
                perror("malloc");
                exit(1);
            }
            board[y + j][x + i]->battleship_id = battleship->id;
            board[y + j][x + i]->hit = 0;
            board[y + j][x + i]->tried = 0;
        }
    }

    return 0;
}

/*Returns 0 if the battleship cell is damaged as the result of the hit and 1 otherwise.
 *Returns -1 if the error occured.*/
int hit_battleship(Battleship_cell*** board, int x, int y) {
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE)
        return -1;

    // already tried this cell
    if (board[y][x] != NULL && board[y][x]->tried == 1)
        return -2;

    // MISS
    if (board[y][x] == NULL) {
        board[y][x] = malloc(sizeof(Battleship_cell));
        board[y][x]->battleship_id = -1;
        board[y][x]->hit = 0;
        board[y][x]->tried = 1;
        return 0;
    }

    // HIT
    board[y][x]->hit = 1;
    board[y][x]->tried = 1;
    return 1;
}

/*Free the memory allocated by the board*/
void free_board(Battleship_cell*** board) {
    if (board == NULL) {
        return;
    }

    for (int i = 0; i < BOARD_SIZE; i++) {
        if (board[i] == NULL) {
            continue;
        }
        for (int j = 0; j < BOARD_SIZE; j++) {
            free(board[i][j]);
        }
        free(board[i]);
    }
    free(board);
}

char* return_board(Battleship_cell*** board, int is_opponent) {
    int size = BOARD_SIZE * (BOARD_SIZE + 1);
    char *board_to_return = malloc(size + 1);

    int k = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {

            // never touched
            if (board[i][j] == NULL) {
                board_to_return[k++] = '.';   // interpunct
            }

            // MISS (dummy cell)
            else if (board[i][j]->battleship_id == -1) {
                board_to_return[k++] = '*';   // or 'o'
            }

            // HIT
            else if (board[i][j]->hit == 1) {
                board_to_return[k++] = 'X';
            }

            // ship not hit
            else {
                if (is_opponent == 0) {
                    board_to_return[k++] = 'O';
                } else {
                    board_to_return[k++] = '.';
                }
            }
        }
        board_to_return[k++] = '\n';
    }

    board_to_return[k] = '\0';
    return board_to_return;
}