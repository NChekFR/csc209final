#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>    /* Internet domain header */
#include <netdb.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include "game_ops.h"
#ifndef PORT
#define PORT 4242
#endif
#define MESSAGE_BUF_SIZE 4096


// gcc -Wall -Wextra -std=c11 player.c -o player
// ./player

ssize_t send_all(int fd, const char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t sent = write(fd, buf + total, len - total);
        if (sent <= 0) {
            return sent;
        }
        total += (size_t)sent;
    }
    return (ssize_t)total;
}

ssize_t read_exact(int fd, char *buf, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, buf + total, len - total);
        if (n <= 0) {
            return n;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t read_until_newline(int fd, char *buf, size_t maxlen) {
    size_t i = 0;
    while (i + 1 < maxlen) {
        char c;
        ssize_t n = read(fd, &c, 1);
        if (n <= 0) {
            return n;
        }
        if (c == '\r') {
            continue;
        }
        buf[i++] = c;
        if (c == '\n') {
            break;
        }
    }
    buf[i] = '\0';
    return (ssize_t)i;
}

void display_legend() {
    // Implementation for displaying the legend explaining the symbols used in the game
    printf("Legend:\n");
    printf(". = unknown\n");
    printf("* = miss\n");
    printf("O = your ship\n");
    printf("X = hit ship\n\n");
}

void display_player_board(const char *board) {
    // Implementation for displaying player's board
    printf("Your board:\n%s\n", board);
}

void display_opponent_board(const char *board) {
    // Implementation for displaying opponent's board
    printf("Opponent board:\n%s\n", board);
}


// Function to receive a message from the server and update the boards accordingly
// first integer is the status of the message (0 for normal message, 1 for error)
int receive_server_message(int soc, char *status, char *action_required, char *message_buf,
                           char *opponent_board, char *player_board) {
    if (read_exact(soc, status, 1) <= 0) {
        return 1;
    }

    if (read_exact(soc, action_required, 1) <= 0) {
        return 1;
    }

    if (read_until_newline(soc, message_buf, MESSAGE_BUF_SIZE) <= 0) {
        return 1;
    }

    if (*status == '0') {
        if (read_exact(soc, opponent_board, BOARD_SERIALIZED_SIZE) <= 0) {
            return 1;
        }
        opponent_board[BOARD_SERIALIZED_SIZE] = '\0';

        if (read_exact(soc, player_board, BOARD_SERIALIZED_SIZE) <= 0) {
            return 1;
        }
        player_board[BOARD_SERIALIZED_SIZE] = '\0';
    }

    return 0;
}

int main() {


    // create socket
    int soc = socket(AF_INET, SOCK_STREAM, 0);
    if (soc == -1) {
        perror("client: socket");
        exit(1);
    }


    //initialize server address    
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    memset(&server.sin_zero, 0, 8);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

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
    the format "x y" where x is the x coordinate and y is the y coordinate of the cell that the player 
    wants to attack. The server will then check if there is a battleship in that cell 
    and update the boards accordingly. The server will also send a message back to the 
    client indicating whether the attack was successful or not. The client will then 
    display the updated boards to the user and wait for the next input. The game will 
    continue until one of the players has all their battleships sunk. The server will 
    then send a message to both clients indicating who won and who lost. The clients 
    will then display the result to the user and exit.
    **/
    char user_input[128];
    char message_buf[MESSAGE_BUF_SIZE];
    char opponent_board[BOARD_SERIALIZED_SIZE + 1];
    char player_board_buf[BOARD_SERIALIZED_SIZE + 1];
    char status;
    char action_required;

    fd_set read_fds, init_fds;

    FD_ZERO(&init_fds);

    FD_SET(STDIN_FILENO, &init_fds);
    FD_SET(soc, &init_fds);

    int max_fd = soc;
    if (STDIN_FILENO > max_fd) {
        max_fd = STDIN_FILENO;
    }
    int retval;

    while (true) {
        read_fds = init_fds;

        retval = select(max_fd + 1, &read_fds, NULL, NULL, NULL);

        if (retval == -1) {
            perror("select");
            break;
        }

        if (FD_ISSET(soc, &read_fds)) {
            if (receive_server_message(soc, &status, &action_required, message_buf, opponent_board, player_board_buf) != 0) {
                break;
            }
            if (status == '0') {
                display_legend();
                display_opponent_board(opponent_board);
                display_player_board(player_board_buf);
            }
            printf("%s", message_buf);

            if (strncmp(message_buf, "You won!", 8) == 0 || strncmp(message_buf, "You lost!", 9) == 0) {
                break;
            }
        }


        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            if (fgets(user_input, sizeof(user_input), stdin) == NULL) {
                break;
            }
            send_all(soc, user_input, strlen(user_input));
        }
    }

    close(soc);
    return 0;
}