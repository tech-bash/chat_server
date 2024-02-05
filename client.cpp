/* Author -: Bhavy Airi Date created :- 29th January 2024
 * Modified by :-  Date Modified :-
 * Code summary :- The client-side code establishes a socket connection,
 * facilitates user authentication, and manages real-time chat interaction with the server, including token-based authentication and message exchange.
 * Modification summary :-
 * config file :-
 * Dependencies :- system libraries mentioned below ..
 * Libraries :- iostream, ctstring .... bits/stdc++.h(it includes all the libraries in itself)
 */

#include <iostream>
#include <cstring>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <bits/stdc++.h>

#define LENGTH 2048

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0;
char name[32];

void str_overwrite_stdout() {
	std::cout << "> " << std::flush;
}
// read_file, fiel
void str_trim_lf(char* arr, int length) {
	for (int i = 0; i < length; i++) { // trim \n
		if (arr[i] == '\n') {
			arr[i] = '\0';
			break;
		}
	}
}

std::string generateToken() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(100000, 999999);
	return std::to_string(dis(gen));
}

void catch_ctrl_c_and_exit(int sig) {
	flag = 1;
}

void authenticate() {
	char username[32];
	char password[32];

	std::cout << "Please enter your username: ";
	std::cin.getline(username, 32);
	str_trim_lf(username, strlen(username));

	std::cout << "Please enter your password: ";
	std::cin.getline(password, 32);
	str_trim_lf(password, strlen(password));

	// Send username and password to the server
	send(sockfd, username, 32, 0);
	send(sockfd, password, 32, 0);

	char response[LENGTH] = {};
	recv(sockfd, response, LENGTH, 0);

	std::cout << response << std::endl;

	if (strcmp(response, "Invalid username or password.") == 0) {
		std::cout << "Authentication failed. Exiting..." << std::endl;
		close(sockfd);
		exit(EXIT_FAILURE);
	} else 	std::cout << "=== WELCOME TO THE CHATROOM ===" << std::endl;
}
// server and cpp header file. _ use kro.
void* send_msg_handler(void* arg) {
    char message[LENGTH] = {};
    char buffer[LENGTH + 32] = {};

    while (1) {
        str_overwrite_stdout();
        std::cin.getline(message, LENGTH);
        str_trim_lf(message, LENGTH);

        if (strcmp(message, "exit") == 0) {
            break;
        } else {
            // Get the current time
            std::time_t currentTime = std::time(nullptr);
            std::string timeStr = std::ctime(&currentTime);
            timeStr.erase(std::remove(timeStr.begin(), timeStr.end(), '\n'), timeStr.end());
            sprintf(buffer, "[%s] %s: %s\n", timeStr.c_str(), name, message);

            send(sockfd, buffer, strlen(buffer), 0);
        }

        memset(message, 0, sizeof(message));
        memset(buffer, 0, sizeof(buffer));
    }
    catch_ctrl_c_and_exit(2);
    return nullptr;
}

void* recv_msg_handler(void* arg) {
	char message[LENGTH] = {};
	while (1) {
		int receive = recv(sockfd, message, LENGTH, 0);
		if (receive > 0) {
			std::cout << message;
			str_overwrite_stdout();
		} else if (receive == 0) {
			break;
		} else {
			// -1
		}
		memset(message, 0, sizeof(message));
	}
	return nullptr;
}
// @toakash, @toall
// server
// user1 to @akash @all...
// seperator
// variable declaring commenting ...

int main(int argc, char **argv) {
	if (argc != 2) {
		std::cout << "Usage: " << argv[0] << " <port>" << std::endl;
		return EXIT_FAILURE;
	}

	const char* ip = "127.0.0.1";
	int port = atoi(argv[1]);

	signal(SIGINT, catch_ctrl_c_and_exit);

	std::cout << "Please enter your name: ";
	std::cin.getline(name, 32);
	str_trim_lf(name, strlen(name));

	if (strlen(name) > 32 || strlen(name) < 2) {
		std::cout << "Name must be less than 30 and more than 2 characters." << std::endl;
		return EXIT_FAILURE;
	}

	struct sockaddr_in server_addr;

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(ip);
	server_addr.sin_port = htons(port);

	// Connect to Server
	int err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (err == -1) {
		std::cout << "ERROR: connect" << std::endl;
		return EXIT_FAILURE;
	}

	// Authenticate with the server
	authenticate();

	pthread_t send_msg_thread;
	if(pthread_create(&send_msg_thread, nullptr, send_msg_handler, nullptr) != 0) {
		std::cout << "ERROR: pthread" << std::endl;
		return EXIT_FAILURE;
	}

	pthread_t recv_msg_thread;
	if(pthread_create(&recv_msg_thread, nullptr, recv_msg_handler, nullptr) != 0) {
		std::cout << "ERROR: pthread" << std::endl;
		return EXIT_FAILURE;
	}

	while (1) {
		if (flag) {
			std::cout << "\nBye" << std::endl;
			break;
		}
	}

	close(sockfd);

	return EXIT_SUCCESS;
}
