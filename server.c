#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "game_ops.h"

#define MESSAGE_BUF_SIZE 4096
#define MESSAGE_SIZE 1024
#define MAX_PLAYERS 2
#define MAX_BATTLESHIPS 5

// gcc -Wall -Wextra -std=c11 server.c game_ops.c -o server
// ./server

int *player_sockets;

Battleship_cell ***player_board[2];

int player_battleships[MAX_PLAYERS][MAX_BATTLESHIPS] = {
    {2, 3, 4, 4, 6},
    {2, 3, 4, 4, 6}
};

int player_scores[MAX_PLAYERS] = {19, 19};

Battleship battleships[] = {
    {.name = "1x2 battleship", .id = 0, .width = 1, .height = 2},
    {.name = "1x3 battleship", .id = 1, .width = 1, .height = 3},
    {.name = "1x4 battleship", .id = 2, .width = 1, .height = 4},
    {.name = "2x2 battleship", .id = 3, .width = 2, .height = 2},
    {.name = "2x3 battleship", .id = 4, .width = 2, .height = 3},
};

fd_set current_sockets, ready_sockets;


int clear_message_buf(char *message_buf) {
    message_buf[0] = '\0';
    return 0;
}

/*
 * first character:
 * 0 -> no parsing error, message followed by boards
 * 1 -> parsing error/info message only
 */
void form_message(char *message_buf, const char *message_to_write, int cur_player, int show_board) {
    clear_message_buf(message_buf);

    size_t used = (size_t) snprintf(
        message_buf,
        MESSAGE_BUF_SIZE,
        "%c%s",
        show_board ? '1' : '0',
        message_to_write
    );

    if (used >= MESSAGE_BUF_SIZE) {
        message_buf[MESSAGE_BUF_SIZE - 1] = '\0';
        return;
    }

    if (show_board == 1) {
        char *opponent_board = return_board(player_board[abs(cur_player - 1)], 1);
        char *cur_player_board = return_board(player_board[cur_player], 0);

        strncat(message_buf, opponent_board, MESSAGE_BUF_SIZE - strlen(message_buf) - 1);
        strncat(message_buf, cur_player_board, MESSAGE_BUF_SIZE - strlen(message_buf) - 1);

        free(opponent_board);
        free(cur_player_board);
    }
}

int parse_response(const char *response_buf, int *parsed_response, int parsed_response_length) {
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
        total += (size_t) sent;
    }
    return (ssize_t) total;
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
    return (ssize_t) i;
}

static void cleanup_and_exit(int exit_code) {
    char close_message_buf[MESSAGE_BUF_SIZE];
    char close_message[MESSAGE_SIZE];
    if (player_board[0] != NULL) {
        free_board(player_board[0]);
        player_board[0] = NULL;
    }
    if (player_board[1] != NULL) {
        free_board(player_board[1]);
        player_board[1] = NULL;
    }

    if (player_sockets != NULL) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (exit_code != -1 && exit_code != 2 && i != exit_code) {
                snprintf(
                    close_message,
                    MESSAGE_BUF_SIZE,
                    "%s%d%s",
                    "Player ",
                    abs(i - 1) + 1,
                    " has disconnected. Game over.\r\n"
                );
                form_message(close_message_buf,
                             close_message,
                             i, 0);
                send_all(player_sockets[i], close_message_buf, strlen(close_message_buf));
            }
            else if (exit_code == -1) {
                form_message(close_message_buf,
                             "The game is over due to the server error.",
                             i, 0);
                send_all(player_sockets[i], close_message_buf, strlen(close_message_buf));
            }
            if (player_sockets[i] >= 0) {
                close(player_sockets[i]);
            }
        }
        free(player_sockets);
    }

    if (exit_code <= 0)
        exit(1);
    exit(0);
}

int max_player_fd() {
    return (player_sockets[0] > player_sockets[1]) ? player_sockets[0] : player_sockets[1];
}

int *connect_to_players() {
    FD_ZERO(&current_sockets);

    player_sockets = malloc(sizeof(int) * MAX_PLAYERS);
    if (player_sockets == NULL) {
        perror("malloc");
        exit(1);
    }

    player_sockets[0] = -1;
    player_sockets[1] = -1;

    int listen_soc = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_soc == -1) {
        perror("server: socket");
        free(player_sockets);
        exit(1);
    }

    int opt = 1;
    if (setsockopt(listen_soc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt");
        close(listen_soc);
        free(player_sockets);
        exit(1);
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(PORT);
    server.sin_addr.s_addr = inet_addr("127.0.0.1");

    if (bind(listen_soc, (struct sockaddr *) &server, sizeof(server)) == -1) {
        perror("server: bind");
        close(listen_soc);
        free(player_sockets);
        exit(1);
    }

    printf("Waiting for players...\n");
    fflush(stdout);

    if (listen(listen_soc, 5) < 0) {
        perror("listen");
        close(listen_soc);
        free(player_sockets);
        exit(1);
    }

    socklen_t client_len = sizeof(struct sockaddr_in);

    struct sockaddr_in player1_addr;
    memset(&player1_addr, 0, sizeof(player1_addr));

    struct sockaddr_in player2_addr;
    memset(&player2_addr, 0, sizeof(player2_addr));

    player_sockets[0] = accept(listen_soc, (struct sockaddr *) &player1_addr, &client_len);
    if (player_sockets[0] == -1) {
        perror("accept");
        close(listen_soc);
        free(player_sockets);
        exit(1);
    }

    printf("First player has connected successfully! Waiting for the second player...\n");
    fflush(stdout);

    char welcome_message_buf[MESSAGE_BUF_SIZE];
    form_message(welcome_message_buf,
                 "You have succesfully connected to the game! Waiting for the second player...\r\n",
                 0, 0);
    send_all(player_sockets[0], welcome_message_buf, strlen(welcome_message_buf));

    FD_SET(player_sockets[0], &current_sockets);

    player_sockets[1] = accept(listen_soc, (struct sockaddr *) &player2_addr, &client_len);
    if (player_sockets[1] == -1) {
        perror("accept");
        close(player_sockets[0]);
        close(listen_soc);
        free(player_sockets);
        exit(1);
    }


    printf("Second player has connected successfully!\n");
    fflush(stdout);

    form_message(welcome_message_buf,
                 "You have succesfully connected to the game! Waiting for the first player to make a move...\r\n",
                 0, 0);
    send_all(player_sockets[1], welcome_message_buf, strlen(welcome_message_buf));

    FD_SET(player_sockets[1], &current_sockets);

    close(listen_soc);
    return player_sockets;
}

int send_setup_prompt(int player_socket, int current_player, int ship_index) {
    char message_buf[MESSAGE_BUF_SIZE];
    char prompt[MESSAGE_BUF_SIZE];

    snprintf(prompt, sizeof(prompt), "Enter the coordinates of the %s\r\n", battleships[ship_index].name);
    form_message(message_buf, prompt, current_player, 1);

    if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
        return -1;
    }
    return 0;
}

int send_move_prompt(int player_socket, int current_player, const char *message_to_opponent) {
    char message_buf[MESSAGE_BUF_SIZE];
    char prompt[MESSAGE_BUF_SIZE];

    snprintf(prompt, sizeof(prompt),
             "%sEnter the coordinates of the cell you want to hit.\r\n",
             message_to_opponent);
    form_message(message_buf, prompt, current_player, 1);

    if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
        return -1;
    }
    return 0;
}

/*
 * Returns:
 *  0 -> battleship created successfully
 *  1 -> invalid input / invalid placement, same player should retry
 * -1 -> disconnect / fatal socket error
 */
int create_battleship(int player_socket, int current_player, int ship_index, char *response_buf) {
    ssize_t n = read_line(player_socket, response_buf, MESSAGE_BUF_SIZE);
    if (n <= 0) {
        printf("Player %d exited\n", current_player + 1);
        fflush(stdout);
        cleanup_and_exit(current_player);
    }

    char message_buf[MESSAGE_BUF_SIZE];
    int parsed_response[3];

    int conversion_error = parse_response(response_buf, parsed_response, 3);
    if (conversion_error == 0 || (parsed_response[2] != 0 && parsed_response[2] != 1)) {
        form_message(
            message_buf,
            "Invalid input. Correct form: [x coordinate] [y coordinate] [orientation].\r\n",
            current_player,
            0);
        if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
            return -1;
        }
        return 1;
    }

    int result = insert_battleship(
        player_board[current_player],
        &battleships[ship_index],
        parsed_response[2],
        parsed_response[0],
        parsed_response[1]
    );

    if (result == 1) {
        form_message(
            message_buf,
            "The battleship cannot be placed here. Please, try another spot.\r\n",
            current_player,
            0);
        if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
            return -1;
        }
        return 1;
    }

    form_message(message_buf, "The battleship has been placed. Waiting for another player...\r\n", current_player, 1);
    if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
        return -1;
    }

    return 0;
}

/*
 * Returns:
 *  0 -> move successful
 *  1 -> invalid input / invalid move, same player should retry
 * -1 -> disconnect / fatal socket error
 */
int handle_player_move(int player_socket, int current_player, char *message_to_opponent, size_t opp_size) {
    int opponent = abs(current_player - 1);
    int parsed_response[2];
    char message_buf[MESSAGE_BUF_SIZE];

    char response_buf[MESSAGE_BUF_SIZE];
    ssize_t n = read_line(player_socket, response_buf, MESSAGE_BUF_SIZE);
    if (n <= 0) {
        printf("Player %d exited\n", current_player + 1);
        fflush(stdout);
        cleanup_and_exit(current_player);
    }

    int conversion_error = parse_response(response_buf, parsed_response, 2);
    if (conversion_error == 0) {
        form_message(message_buf, "Invalid input. Correct form: [x coordinate] [y coordinate].\r\n",
                     current_player, 0);
        if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
            return -1;
        }
        return 1;
    }

    int result_of_the_hit = hit_battleship(player_board[opponent], parsed_response[0], parsed_response[1]);

    if (result_of_the_hit == -1) {
        form_message(message_buf, "Invalid coordinates. Please, try again.\r\n", current_player, 0);
        if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
            return -1;
        }
        return 1;
    }

    if (result_of_the_hit == -2) {
        form_message(message_buf, "You already tried this cell. Choose another.\r\n", current_player, 0);
        if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
            return -1;
        }
        return 1;
    }

    if (result_of_the_hit == 1) {
        int battleship_id = player_board[opponent][parsed_response[1]][parsed_response[0]]->battleship_id;

        player_battleships[opponent][battleship_id]--;
        player_scores[opponent]--;

        if (player_battleships[opponent][battleship_id] == 0) {
            snprintf(message_to_opponent, opp_size, "Your battleship was destroyed! ");
            form_message(message_buf, "You destroyed a battleship! Waiting for another player...\r\n", current_player,
                         1);
        } else {
            snprintf(message_to_opponent, opp_size, "Your battleship was damaged! ");
            form_message(message_buf, "You hit a battleship! Waiting for another player...\r\n", current_player, 1);
        }
    } else {
        snprintf(message_to_opponent, opp_size, "Your opponent has missed! ");
        form_message(message_buf, "You missed a battleship! Waiting for another player...\r\n", current_player, 1);
    }

    if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
        return -1;
    }

    return 0;
}

int wrong_turn_message(int player_socket, int current_player) {
    char message_buf[MESSAGE_BUF_SIZE];
    form_message(message_buf,
                 "It is not your turn to make a move now. Please, wait for another player.\r\n",
                 current_player,
                 0);
    if (send_all(player_socket, message_buf, strlen(message_buf)) <= 0) {
        return -1;
    }
    return 0;
}

int check_game_over() {
    return (player_scores[0] == 0 || player_scores[1] == 0) ? 1 : 0;
}

int main() {
    int game_over = 0;
    int player_wins = 0;
    int current_player_turn = 0;

    player_board[0] = initialize_board();
    player_board[1] = initialize_board();

    int *player_sockets = connect_to_players();

    int player_battleships_num[MAX_PLAYERS] = {0, 0};

    while (player_battleships_num[0] < MAX_BATTLESHIPS || player_battleships_num[1] < MAX_BATTLESHIPS) {
        int move_made = 0;
        int ship_index = player_battleships_num[current_player_turn];

        if (send_setup_prompt(player_sockets[current_player_turn], current_player_turn, ship_index) < 0) {
            printf("Player %d exited\n", current_player_turn + 1);
            fflush(stdout);
            cleanup_and_exit(current_player_turn);
        }

        while (move_made == 0) {
            ready_sockets = current_sockets;
            if (select(max_player_fd() + 1, &ready_sockets, NULL, NULL, NULL) < 0) {
                perror("select");
                cleanup_and_exit(-1);
            }

            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!FD_ISSET(player_sockets[i], &ready_sockets)) {
                    continue;
                }

                char response_buf[MESSAGE_BUF_SIZE];
                if (i != current_player_turn) {
                    char discard_buf[MESSAGE_BUF_SIZE];
                    ssize_t n = read_line(player_sockets[i], discard_buf, sizeof(discard_buf));
                    if (n <= 0) {
                        printf("Player %d exited\n", i + 1);
                        fflush(stdout);
                        cleanup_and_exit(i);
                    }
                    if (wrong_turn_message( player_sockets[i], i) < 0) {
                        printf("Player %d exited\n", i + 1);
                        fflush(stdout);
                        cleanup_and_exit(i);
                    }
                    continue;
                }

                int result = create_battleship(player_sockets[i], i, ship_index, response_buf);
                if (result < 0) {
                    cleanup_and_exit(-1);
                }
                if (result == 0) {
                    player_battleships_num[i]++;
                    current_player_turn = abs(current_player_turn - 1);
                    move_made = 1;
                }
                break;
            }
            FD_ZERO(&ready_sockets);
        }
    }

    /* Main game loop */
    char message_to_opponent[MESSAGE_BUF_SIZE] = "";
    current_player_turn = 0;

    while (game_over == 0) {
        if (send_move_prompt(player_sockets[current_player_turn], current_player_turn, message_to_opponent) < 0) {
            printf("Player %d exited\n", current_player_turn + 1);
            fflush(stdout);
            cleanup_and_exit(current_player_turn);
        }

        int move_made = 0;

        while (move_made == 0 && game_over == 0) {
            ready_sockets = current_sockets;
            if (select(max_player_fd() + 1, &ready_sockets, NULL, NULL, NULL) < 0) {
                perror("select");
                cleanup_and_exit(-1);
            }

            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!FD_ISSET(player_sockets[i], &ready_sockets)) {
                    continue;
                }

                if (i != current_player_turn) {
                    char discard_buf[MESSAGE_BUF_SIZE];
                    ssize_t n = read_line(player_sockets[i], discard_buf, sizeof(discard_buf));
                    if (n <= 0) {
                        printf("Player %d exited\n", i + 1);
                        fflush(stdout);
                        cleanup_and_exit(i);
                    }
                    if (wrong_turn_message(player_sockets[i], i) < 0) {
                        printf("Player %d exited\n", i + 1);
                        fflush(stdout);
                        cleanup_and_exit(i);
                    }
                    continue;
                }

                int result = handle_player_move(
                    player_sockets[i],
                    i,
                    message_to_opponent,
                    sizeof(message_to_opponent)
                );

                if (result < 0) {
                    cleanup_and_exit(-1);
                }

                if (result == 0) {
                    game_over = check_game_over();
                    current_player_turn = abs(current_player_turn - 1);
                    move_made = 1;
                }
                break;
            }
        }
    }

    if (player_scores[0] == 0) {
        player_wins = 1;
    } else {
        player_wins = 0;
    }

    char message_buf[MESSAGE_BUF_SIZE];

    form_message(message_buf, "You won!\r\n", player_wins, 1);
    if (send_all(player_sockets[player_wins], message_buf, strlen(message_buf)) <= 0) {
        cleanup_and_exit(2);
    }

    form_message(message_buf, "You lost!\r\n", abs(player_wins - 1), 1);
    if (send_all(player_sockets[abs(player_wins - 1)], message_buf, strlen(message_buf)) <= 0) {
        cleanup_and_exit(2);
    }

    free_board(player_board[0]);
    free_board(player_board[1]);
    player_board[0] = NULL;
    player_board[1] = NULL;

    close(player_sockets[0]);
    close(player_sockets[1]);
    free(player_sockets);

    return 0;
}
