#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "game_entities.h"

#define BOARD_SIZE 10

void initialize_board(Battleship_cell*** board) {
    board = malloc(BOARD_SIZE * sizeof(Battleship_cell*));
    for (int i = 0; i < BOARD_SIZE; i++) {
        board[i] = malloc(BOARD_SIZE * sizeof(Battleship_cell));
    }
}

int fill_board(Battleship_cell*** board, int player_id) {
    int x, y;
    char battleship_orientation;
    int result = scanf("%d %d %c", &x, &y,
        &battleship_orientation);
    if (result < 3) {
        printf("Invalid input. Required format: [coordinate x] [coordinate y] [orientation]\n");
        return 1;
    }
    if (board[y][x] != NULL) {
        printf("This cell is already occupied\n");
        return 1;
    }
}

int fill_pixel(Battleship_cell*** board, int x, int y,
    int battleship_id, int battleship_orientation) {
    if (board[y][x] != NULL)
        return 1;
    board[y][x] = malloc(sizeof(Battleship_cell));
    board[y][x]->battleship_id
}