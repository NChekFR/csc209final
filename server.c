#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>    /* Internet domain header */
#include <arpa/inet.h>     /* only needed on my mac */
#include "game_entities.h"

int main() {


    Battleship_cell*** player1_board;
    Battleship_cell*** player2_board;
    initialize_board(player1_board);
    initialize_board(player2_board);



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


    int player_1_socket = accept(listen_soc, (struct sockaddr *)&player1_addr, &client_len);
    if (player_1_socket == -1) {
        perror("accept");
        return -1;
    } 

    int player_2_socket = accept(listen_soc, (struct sockaddr *)&player2_addr, &client_len);
    if (player_2_socket == -1) {
        perror("accept");
        return -1;
    }

    close(listen_soc);





    while (condition)
    {
        /* code */
    }
    

/*
    write(client_socket, "hello\r\n", 7);

    char line[10];
    read(client_socket, line, 10);
     before we can use line in a printf statement, ensure it is a string 
    line[9] = '\0';
    printf("I read %s\n", line);

    return 0;

*/
}
