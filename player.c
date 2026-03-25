#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "game_entities.h"
#define BOARD_SIZE 10


int main() {



    //initialize 2 boards, one for self and one for opponent




    // create socket
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc == -1) {
        perror("client: socket");
        exit(1);
    }


    //initialize server address    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(54321);  
    memset(&server.sin_zero, 0, 8);
    
    struct addrinfo *ai;
    char * hostname = "localhost";

    /* this call declares memory and populates ailist */
    getaddrinfo(hostname, NULL, NULL, &ai);
    server.sin_addr = ((struct sockaddr_in *) ai->ai_addr)->sin_addr;


    // free the memory that was allocated by getaddrinfo for this list
    freeaddrinfo(ai);

    int ret = connect(soc, (struct sockaddr *)&server, sizeof(struct sockaddr_in));
    if (ret == -1) {
        perror("client: connect");
        exit(1);
    }

    /**
    When reading the battleship cells, first 100 are the opponent's board 
    and the next 100 are the player's board. Each cell is represented by a struct
    with the battleship id and whether it is hit or not. The client will read the 
    whole board every time and then display it to the user. The client will also 
    read a message from the user and send it to the server. The message will be in 
    the format "x y" where x and y are the coordinates of the cell that the player 
    wants to attack. The server will then check if there is a battleship in that cell 
    and update the boards accordingly. The server will also send a message back to the 
    client indicating whether the attack was successful or not. The client will then 
    display the updated boards to the user and wait for the next input. The game will 
    continue until one of the players has all their battleships sunk. The server will 
    then send a message to both clients indicating who won and who lost. The clients 
    will then display the result to the user and exit.
    **/
    Battleship_cell *** board_message_buf = malloc(200* sizeof(Battleship_cell));
    char ** message_buf = malloc(200* sizeof(char*));
    

    // only 5 ships so only needs to be done 5 times
    for (int i = 0; i < 5; i++) {
        // send and receive positions of your ships 
        read(soc, board_message_buf, sizeof(board_message_buf));

        read(STIDIN, message_buf, sizeof(message_buf));
        write(soc, message_buf, sizeof(message_buf));
    }

    int *game_status_message_buf = malloc(sizeof(int));
    

    int attempted_spots[10][10];
    memset(attempted_spots, 0, sizeof(attempted_spots));

    while (1) {
        read(soc, board_message_buf, sizeof(board_message_buf));
        read(soc, game_status_message_buf, sizeof(game_status_message_buf));
        if (*game_status_message_buf != 0) {
            break;
        }

        display_legend();
        display_opponent_board(board_message_buf);
        display_player_board(board_message_buf);

        int x, y;
        char input_buf[128];
        char send_buf[32];

        while (1) {
            printf("Enter attack coordinates (x y) in range 0..9: ");
            if (fgets(input_buf, sizeof(input_buf), stdin) == NULL) {
                printf("Input error. Try again.\n");
                continue;
            }

            if (sscanf(input_buf, "%d %d", &x, &y) != 2) {
                printf("Invalid format. Use two numbers: x y\n");
                continue;
            }

            if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
                printf("Coordinates out of range (0..9).\n");
                continue;
            }

            if (attempted_spots[y][x]) {
                printf("Already attempted (%d %d). Pick a different target.\n", x, y);
                continue;
            }

            attempted_spots[y][x] = 1;
            break;
        }

        snprintf(send_buf, sizeof(send_buf), "%d %d", x, y);
        write(soc, send_buf, strlen(send_buf) + 1);

        // wait for server response to attack
        if (read(soc, input_buf, sizeof(input_buf)) > 0) {
            printf("Server response: %s\n", input_buf);
        } else {
            printf("Server closed connection or read error.\n");
            break;
        }
    }

    // server will send 1 and -1 depending on which player won
    if (game_status_message_buf == 1) {
        printf("You win!");
    } else {
        printf("You lose!");
    }
    




/*
    printf("Connect returned %d\n", ret);

    char buf[10];
    read(soc, buf, 7);
    buf[7] = '\0';
    printf("I read %s\n", buf);

    write(soc, "0123456789", 10);
    return 0;

*/
}

void display_legend() {
    // Implementation for displaying the legend explaining the symbols used in the game
}

void display_player_board(Battleship_cell*** board) {
    // Implementation for displaying player's board
    // last 100 cells of the board_message_buf are the player's board

    printf("Your board:\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            Battleship_cell cell = board[i][j];
            if (cell == NULL) {
                printf("· ");
            } else if (cell.hit == 0) {
                printf("O ");
            } else {
                printf("X ");
            }
        }
        printf("\n");
    }
}

void display_opponent_board(Battleship_cell*** board) {
    // Implementation for displaying opponent's board
    // first 100 cells of the board_message_buf are the opponent's board

    printf("Opponent's board:\n");
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            Battleship_cell cell = board[i][j];
            if (cell == NULL) {
                printf("· ");
            } else if (cell.hit == 0) {
                printf("· ");
            } else {
                printf("X ");
            }
        }
        printf("\n");
    }
    
}