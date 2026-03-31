#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */
#include <arpa/inet.h>     /* only needed on my mac */
#include "game_ops.h"



// gcc -Wall -Wextra -std=c11 server.c game_ops.c -o server
// ./server

Battleship_cell*** player_board[2];

int player_battleships[2][5] = {{2, 3, 4, 4, 6}, {2, 3, 4, 4, 6}};
int player_scores[2] = {19, 19};
Battleship battleships[] = {
    {.name = "1x2 battleship", .id = 0, .width = 1, .height = 2},
    {.name = "1x3 battleship", .id = 1, .width = 1, .height = 3},
    {.name = "1x4 battleship", .id = 2, .width = 1, .height = 4},
    {.name = "2x2 battleship", .id = 3, .width = 2, .height = 2},
    {.name = "2x3 battleship", .id = 4, .width = 2, .height = 3},
};

#define MESSAGE_BUF_SIZE 4096

int clear_message_buf(char* message_buf) {
    message_buf[0] = '\0';
    return 0;
}

/*
 * first character:
 * 0 -> no parsing error, message followed by boards
 * 1 -> parsing error, message only
 */
void form_message(char *message_buf, const char *message_to_write, int cur_player, int has_parsing_error) {
    clear_message_buf(message_buf);

    size_t used = (size_t)snprintf(message_buf, MESSAGE_BUF_SIZE, "%c%s", has_parsing_error ? '1' : '0', message_to_write);
    if (used >= MESSAGE_BUF_SIZE) {
        message_buf[MESSAGE_BUF_SIZE - 1] = '\0';
        return;
    }

    if (has_parsing_error == 0) {
        char* opponent_board = return_board(player_board[abs(cur_player - 1)], 1);
        char* cur_player_board = return_board(player_board[cur_player], 0);

        strncat(message_buf, opponent_board, MESSAGE_BUF_SIZE - strlen(message_buf) - 1);
        strncat(message_buf, cur_player_board, MESSAGE_BUF_SIZE - strlen(message_buf) - 1);

        free(opponent_board);
        free(cur_player_board);
    }
}

int parse_response(char* response_buf, int* parsed_response, int parsed_response_length) {
    if (parsed_response_length == 3) {
        int x, y, orientation;
        if (sscanf(response_buf, "%d %d %d", &x, &y, &orientation) != 3) {
            return 0;
        }
        parsed_response[0] = x;
        parsed_response[1] = y;
        parsed_response[2] = orientation;
        return 1;
    }

    if (parsed_response_length == 2) {
        int x, y;
        if (sscanf(response_buf, "%d %d", &x, &y) != 2) {
            return 0;
        }
        parsed_response[0] = x;
        parsed_response[1] = y;
        return 1;
    }

    return 0;
}

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

ssize_t read_line(int fd, char *buf, size_t maxlen) {
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

int* connect_to_players() {
    int* player_sockets = malloc(sizeof(int) * 2);
    if (player_sockets == NULL) {
        perror("malloc");
        exit(1);
    }

    // create socket
    int listen_soc = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_soc == -1) {
        perror("server: socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(listen_soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(listen_soc);
        exit(1);
    }

    //initialize server address
    struct sockaddr_in server;
    server.sin_family = AF_INET;
    server.sin_port = htons(54321);
    memset(&server.sin_zero, 0, 8);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    // bind socket to an address
    if (bind(listen_soc, (struct sockaddr *) &server, sizeof(struct sockaddr_in)) == -1) {
        perror("server: bind");
        close(listen_soc);
        exit(1);
    }

    // Set up a queue in the kernel to hold pending connections.
    if (listen(listen_soc, 5) < 0) {
        // listen failed
        perror("listen");
        close(listen_soc);
        exit(1);
    }

    socklen_t client_len = sizeof(struct sockaddr_in);

    struct sockaddr_in player1_addr;
    player1_addr.sin_family = AF_INET;

    struct sockaddr_in player2_addr;
    player2_addr.sin_family = AF_INET;

    player_sockets[0] = accept(listen_soc, (struct sockaddr *)&player1_addr, &client_len);
    if (player_sockets[0] == -1) {
        perror("accept");
        close(listen_soc);
        exit(-1);
    }
	printf("Player 1 connected, waiting for player 2 to connect.\n");
	fflush(stdout);

    player_sockets[1] = accept(listen_soc, (struct sockaddr *)&player2_addr, &client_len);
    if (player_sockets[1] == -1) {
        perror("accept");
        close(listen_soc);
        exit(-1);
    }
    close(listen_soc);
    return player_sockets;
}

int main() {

    //Conditions for game over. game_over is 1 when the game is over. player_wins 0 is when the first player wins, and
    // 1 when the other player wins
    int game_over = 0;
    int player_wins = 0;

    //Initialize boards for both players
    player_board[0] = initialize_board();
    player_board[1] = initialize_board();

    //Connect to player clients
    int* player_sockets = connect_to_players();

    //Buffer for the message sent from server to the client
    char message_buf[MESSAGE_BUF_SIZE];

    //Buffer for the message sent from client to the server
    char response_buf[1024];

    //correct_response is 1 when the response from the client aligns with the required type and 0 otherwise
    int correct_response = 0;

    //Loop over the list of battleships for both players to initialize them
    for (int i = 0; i < 5; i++) {

        //Loop over clients for each battleship
        for (int j = 0; j < 2; j++) {
            int parsed_response[3];
            correct_response = 0;

            while (correct_response == 0) {
                char prompt[MESSAGE_BUF_SIZE];
                snprintf(prompt, sizeof(prompt), "Enter the coordinates of the %s\r\n", battleships[i].name);
                form_message(message_buf, prompt, j, 0);
                send_all(player_sockets[j], message_buf, strlen(message_buf));

                if (read_line(player_sockets[j], response_buf, sizeof(response_buf)) <= 0) {
                    close(player_sockets[0]);
                    close(player_sockets[1]);
                    free_board(player_board[0]);
                    free_board(player_board[1]);
                    free(player_sockets);
                    return 1;
                }

                int convertion_error = parse_response(response_buf, parsed_response, 3);
                if (convertion_error == 0 || (parsed_response[2] != 0 && parsed_response[2] != 1)) {
                    form_message(message_buf, "Invalid input. Correct form: [x coordinate] [y_coordinate] [orientation].\r\n",
                        j, 1);
                    send_all(player_sockets[j], message_buf, strlen(message_buf));
                    continue;
                }

                //Attempt to place the battleship on the given location
                int result = insert_battleship(player_board[j], &battleships[i], parsed_response[2],
                    parsed_response[0], parsed_response[1]);
                //If the battleship can be placed, change the loop condition for it to terminate
                if (result == 0) {
                    correct_response = 1;
                }
                //If the battleship cannot be placed, form a signaling message to the client and do not terminate the loop
                else {
                    form_message(message_buf, "The battleship cannot be placed here. Please, try another spot.\r\n",
                        j, 0);
                    send_all(player_sockets[j], message_buf, strlen(message_buf));
                }
            }
            form_message(message_buf, "The battleship has been placed.\r\n", j, 0);
            send_all(player_sockets[j], message_buf, strlen(message_buf));
        }
    }

    //Message to be sent to another player with the description of the results of actions taken by the first player
    char message_to_opponent[256] = "";
    correct_response = 0;
    int result_of_the_hit = 0;
    //Main game loop
    while (game_over == 0) {
        for (int j = 0; j < 2; j++) {
            int parsed_response[2];
            correct_response = 0;

            while (correct_response == 0) {
                char message_to_write[MESSAGE_BUF_SIZE];
                snprintf(message_to_write, sizeof(message_to_write), "%sEnter the coordinates of the cell you want to hit.\r\n",
                         message_to_opponent);
                form_message(message_buf, message_to_write, j, 0);

                //Send a message to the player asking him to enter the coordinates of the battleship he wants to hit
                send_all(player_sockets[j], message_buf, strlen(message_buf));

                //Loop while the response from the player is of the incorrect format
                if (read_line(player_sockets[j], response_buf, sizeof(response_buf)) <= 0) {
                    close(player_sockets[0]);
                    close(player_sockets[1]);
                    free_board(player_board[0]);
                    free_board(player_board[1]);
                    free(player_sockets);
                    return 1;
                }

                int convertion_error = parse_response(response_buf, parsed_response, 2);
                if (convertion_error == 0) {
                    form_message(message_buf, "Invalid input. Correct form: [x coordinate] [y_coordinate].\r\n",
                        j, 1);
                    send_all(player_sockets[j], message_buf, strlen(message_buf));
                    continue;
                }

                int opponent = abs(j - 1);
                result_of_the_hit = hit_battleship(player_board[opponent], parsed_response[0], parsed_response[1]);

                if (result_of_the_hit == -1) {
                    form_message(message_buf, "Invalid coordinates. Please, try again.\r\n",
                        j, 0);
                    send_all(player_sockets[j], message_buf, strlen(message_buf));
                    continue;
                }

                if (result_of_the_hit == -2) {
                    form_message(message_buf, "You already tried this cell. Choose another.\r\n",
                        j, 0);
                    send_all(player_sockets[j], message_buf, strlen(message_buf));
                    continue;
                }
                correct_response = 1;
            }

            //Player was able to hit the opponents battleship
            if (result_of_the_hit == 1) {
                int opponent = abs(j - 1);
                int battleship_id = player_board[opponent][parsed_response[1]][parsed_response[0]]->battleship_id;

                // decrement the battleship's health 
                player_battleships[opponent][battleship_id]--;
                player_scores[opponent]--;

                // first: check game over
                if (player_scores[opponent] == 0) {
                    game_over = 1;
                    player_wins = j;

                    snprintf(message_to_opponent, sizeof(message_to_opponent),
                            "Your last battleship was destroyed! ");
                    form_message(message_buf,
                                "You destroyed the last battleship and won the game!\r\n", j, 0);
                    send_all(player_sockets[j], message_buf, strlen(message_buf));
                    break;
                }

                // THEN: check if ship destroyed
                if (player_battleships[opponent][battleship_id] == 0) {
                    snprintf(message_to_opponent, sizeof(message_to_opponent),
                            "Your battleship was destroyed! ");
                    form_message(message_buf,
                                "You destroyed a battleship!\r\n", j, 0);
                }
                else {
                    snprintf(message_to_opponent, sizeof(message_to_opponent),
                            "Your battleship was damaged! ");
                    form_message(message_buf,
                                "You hit a battleship!\r\n", j, 0);
                }
            }   

            //If the player has missed
            else if (result_of_the_hit == 0) {
                snprintf(message_to_opponent, sizeof(message_to_opponent), "Your opponent has missed! ");
                form_message(message_buf, "You missed a battleship!\r\n", j, 0);
            }

            send_all(player_sockets[j], message_buf, strlen(message_buf));
        }
    }

    if (player_wins == 0) {
        form_message(message_buf, "You won!\r\n", player_wins, 0);
        send_all(player_sockets[player_wins], message_buf, strlen(message_buf));
        form_message(message_buf, "You lost!\r\n", abs(player_wins - 1), 0);
        send_all(player_sockets[abs(player_wins - 1)], message_buf, strlen(message_buf));
    }
    else {
        form_message(message_buf, "You lost!\r\n", player_wins, 0);
        send_all(player_sockets[player_wins], message_buf, strlen(message_buf));
        form_message(message_buf, "You won!\r\n", abs(player_wins - 1), 0);
        send_all(player_sockets[abs(player_wins - 1)], message_buf, strlen(message_buf));
    }

    free_board(player_board[0]);
    free_board(player_board[1]);
    close(player_sockets[0]);
    close(player_sockets[1]);
    free(player_sockets);
    return 0;
}