#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */
#include <arpa/inet.h>     /* only needed on my mac */
#include "game_entities.h"
#include "game_ops.c"

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

int clear_message_buf(char* message_buf) {
    for (int i = 0; i < 1024; i++) {
        message_buf[i] = '\0';
    }
}

void form_message(char *message_buf, char *message_to_write, int cur_player, int success) {
    clear_message_buf(message_buf);
    strncat(message_buf, success ? "0\r\n" : "1\r\n", 3);
    strncat(message_buf, message_to_write, strlen(message_to_write) + 2);
    char* cur_player_board = return_board(player_board[cur_player], 0);
    char* opponent_board = return_board(player_board[abs(cur_player - 1)], 1);
    strncat(message_buf, cur_player_board, strlen(cur_player_board) + 2);
    strncat(message_buf, opponent_board, strlen(opponent_board) + 2);
}

int parse_response(char* message_buf, char* response_buf, int* parsed_response, int parsed_response_length) {
    char* endptr = NULL;
    strcpy(endptr, response_buf);
    for (int k = 0; k < parsed_response_length; k++) {
        parsed_response[k] = strtol(&response_buf[k], &endptr, 10);
        if (response_buf == endptr) {
            form_message(message_buf, "Invalid input. "
                                      "Correct form: [x coordinate] [y_coordinate] [orientation].\r\n",
                j, 0);
            write(player_sockets[j], message_buf, strlen(message_buf));
            return 1;
        }
    }
    return 0;
}

int* connect_to_players() {
    int* player_sockets = malloc(sizeof(int) * 2);
    // create socket
    int listen_soc = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_soc == -1) {
        perror("server: socket");
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
        exit(1);
    }

    unsigned int client_len = sizeof(struct sockaddr_in);

    struct sockaddr_in player1_addr;
    player1_addr.sin_family = AF_INET;

    struct sockaddr_in player2_addr;
    player2_addr.sin_family = AF_INET;


    player_sockets[0] = accept(listen_soc, (struct sockaddr *)&player1_addr, &client_len);
    if (player_sockets[0] == -1) {
        perror("accept");
        exit(-1);
    }

    player_sockets[1] = accept(listen_soc, (struct sockaddr *)&player2_addr, &client_len);
    if (player_sockets[1] == -1) {
        perror("accept");
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
    initialize_board(player_board[0]);
    initialize_board(player_board[1]);

    //Connect to player clients
    int* player_sockets = connect_to_players();

    //Buffer for the message sent from server to the client
    char message_buf[1024];

    //Buffer for the message sent from client to the server
    char response_buf[1024];

    //correct_response is 1 when the response from the client aligns with the required type and 0 otherwise
    int correct_response = 0;

    //Loop over the list of battleships for both players to initialize them
    for (int i = 0; i < 5; i++) {

        //Loop over clients for each battleship
        for (int j = 0; j < 2; j++) {
            int parsed_response[3];
            clear_message_buf(message_buf);
            strcat(message_buf, "0");
            strcat(message_buf, "Enter the coordintaes of the ");
            strcat(message_buf, battleships[i].name);

            //Send the message asking for player to send his coordinates
            write(player_sockets[j], message_buf, strlen(message_buf));

            //Loop while the response from the player is of the incorrect format
            while (correct_response == 0) {
                read(player_sockets[j], response_buf, 3);
                int convertion_error = parse_response(message_buf, response_buf, parsed_response, 3);
                if (convertion_error == 1)
                    continue;

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
                    write(player_sockets[j], message_buf, strlen(message_buf));
                }
            }
            form_message(message_buf, "The battleship has been placed.\r\n", j, 1);
            write(player_sockets[j], message_buf, strlen(message_buf));
        }
    }

    //Message to be sent to another player with the description of the results of actions taken by the first player
    char *message_to_opponent = "";
    char *message_to_write = malloc(strlen(message_to_opponent) + 55);
    correct_response = 0;
    int result_of_the_hit = 0;
    //Main game loop
    while (game_over == 0) {
        for (int j = 0; j < 2; j++) {
            int parsed_response[2];
            strcpy(message_to_write, message_to_opponent);
            strcat(message_to_write, "Enter the coordinates of the cell you want to hit.\r\n");
            form_message(message_buf, message_to_write,j, 1);

            //Send a message to the player asking him to enter the coordinates of the battleship he wants to hit
            write(player_sockets[j], message_buf, strlen(message_buf));

            //Loop while the response from the player is of the incorrect format
            while (correct_response == 0) {
                read(player_sockets[j], response_buf, 2);
                int convertion_error = parse_response(message_buf, response_buf, parsed_response, 2);
                if (convertion_error == 1)
                    continue;
                result_of_the_hit = hit_battleship(player_board[j], parsed_response[0], parsed_response[1]);
                if (result_of_the_hit == -1) {
                    form_message(message_buf, "Invalid coordinates. Please, try again.\r\n",
                        j, 0);
                    continue;
                }
                correct_response = 1;
            }

            //Player was able to hit the opponents battleship
            if (result_of_the_hit == 1) {
                int battleship_id = player_board[j][parsed_response[0]][parsed_response[1]]->battleship_id;

                //Decrease the number of cells corresponding the 'living' parts of the battleship
                player_battleships[j][battleship_id]--;

                //Decrease the number of the overall 'living; parts of all of the battleships that belong to a player
                player_scores[j]--;

                //If there are no more 'living' parts that correspond to a battleship
                if (player_battleships[j][battleship_id] == 0) {
                    message_to_opponent = "Your battleship was destroyed! ";
                    form_message(message_buf, "You destroyed a battleship!\r\n", j, 1);
                }

                //If all the battleships belonging to player were destroyed
                else if (player_scores[j] == 0) {
                    game_over = 1;
                    player_wins = j;
                    break;
                }

                //If the player was able to hit the battleship, but it still has 'living' parts
                else {
                    message_to_opponent = "Your battleship was damaged! ";
                    form_message(message_buf, "You hit a battleship!\r\n", j, 1);
                }
            }

            //If the player has missed
            else if (result_of_the_hit == 0) {
                message_to_opponent = "Your opponent has missed! ";
                form_message(message_buf, "You missed a battleship!\r\n", j, 1);
            }
            write(player_sockets[j], message_buf, strlen(message_buf));
        }
    }

    free(message_to_write);

    if (player_wins == 0) {
        form_message(message_buf, "You won!\r\n", player_wins, 1);
        write(player_sockets[player_wins], message_buf, strlen(message_buf));
        form_message(message_buf, "You lost!\r\n", abs(player_wins - 1), 1);
        write(player_sockets[abs(player_wins - 1)], message_buf, strlen(message_buf));
    }
    else {
        form_message(message_buf, "You lost!\r\n", player_wins, 1);
        write(player_sockets[player_wins], message_buf, strlen(message_buf));
        form_message(message_buf, "You won!\r\n", abs(player_wins - 1), 1);
        write(player_sockets[abs(player_wins - 1)], message_buf, strlen(message_buf));
    }

}
