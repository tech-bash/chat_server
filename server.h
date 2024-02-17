/* Author -: Bhavy Airi Date created :- 9th Feburary 2024
 * Modified by :- Date Modified :-
 * Code summary : This header file holds the structure whose values are used in the server.cpp file ..
 * config file :-
 * Dependencies :-
 * Libraries :-
 */

#ifndef SERVER_H
#define SERVER_H

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define TOKEN_LENGTH 16

/* Client structure */
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
    char token[TOKEN_LENGTH + 1]; // Add token field
    bool in_loop;
} client_t;


#endif /* SERVER_H */


