/* Author -: Bhavy Airi Date created :- 29th January 2024
 * Modified by :-  Date Modified :-
 * Code summary :The server code provided establishes a TCP/IP socket connection, manages multiple clients in a chatroom environment,
 * authenticates users with usernames and passwords, assigns tokens upon successful authentication, and facilitates message exchange among clients while handling concurrency using pthreads in C++.-
 * Modification summary :-
 * config file :-
 * Dependencies :- system libraries mentioned below ..
 * Libraries :- iostream, ctstring, unordered_map etc.
 */

#include <iostream>
#include <cstring>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <unordered_map>
#include <random>
#include <iomanip>

#define MAX_CLIENTS 100
#define BUFFER_SZ 2048
#define TOKEN_LENGTH 16

std::atomic<unsigned int> cli_count{0};
int uid = 10;

/* Client structure */
typedef struct {
    struct sockaddr_in address;
    int sockfd;
    int uid;
    char name[32];
    char token[TOKEN_LENGTH + 1]; // Add token field
} client_t;

std::vector<client_t*> clients;
std::mutex clients_mutex;

// Map to store usernames and passwords
std::unordered_map<std::string, std::string> users = {
    {"user1", "password1"},
    {"user2", "password2"},
    {"user3", "password3"},
    {"user4", "password4"},
    {"user5", "password5"}
};

void str_overwrite_stdout() {
    std::cout << "\r" << "> " << std::flush;
}

void str_trim_lf(char* arr, int length) {
    for (int i = 0; i < length; i++) {
        if (arr[i] == '\n') {
            arr[i] = '\0';
            break;
        }
    }
}

void print_client_addr(struct sockaddr_in addr) {
    std::cout << (addr.sin_addr.s_addr & 0xff) << "." << ((addr.sin_addr.s_addr & 0xff00) >> 8) << "."
              << ((addr.sin_addr.s_addr & 0xff0000) >> 16) << "." << ((addr.sin_addr.s_addr & 0xff000000) >> 24);
}

// Function to generate a random token
std::string generate_token() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis('0', '9');

    std::string token(TOKEN_LENGTH, '\0');
    for (int i = 0; i < TOKEN_LENGTH; ++i) {
        token[i] = dis(gen);
    }

    return token;
}

/* Add clients to queue */
void queue_add(client_t* cl) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    clients.push_back(cl);
}

/* Remove clients to queue */
void queue_remove(int uid) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        if ((*it)->uid == uid) {
            clients.erase(it);
            break;
        }
    }
}

/* Send message to all clients except sender */
void send_message(const char* s, int uid) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& client : clients) {
        if (client->uid != uid) {
            if (write(client->sockfd, s, strlen(s)) < 0) {
                perror("ERROR: write to descriptor failed");
                break;
            }
        }
    }
}

void send_message_to_user(const char* message, const char* username) {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto& client : clients) {
        if (strcmp(client->name, username) == 0) {
            if (write(client->sockfd, message, strlen(message)) < 0) {
                perror("ERROR: write to descriptor failed");
                break;
            }
        }
    }
}

/* Handle all communication with the client */
void* handle_client(void* arg) {
    char buff_out[BUFFER_SZ];
    char name[32];
    int leave_flag = 0;

    cli_count++;
    client_t* cli = static_cast<client_t*>(arg);

    // Authentication
    bool authenticated = false;
    std::string username, password;
    while (!authenticated) {
        // Receive username
        if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1) {
            std::cout << "Failed to receive username." << std::endl;
            leave_flag = 1;
            break;
        }
        username = name;

        // Receive password
        if (recv(cli->sockfd, name, 32, 0) <= 0 || strlen(name) < 2 || strlen(name) >= 32 - 1) {
            std::cout << "Failed to receive password." << std::endl;
            leave_flag = 1;
            break;
        }
        password = name;

        // Check username and password
        auto it = users.find(username);
        if (it != users.end() && it->second == password) {
            authenticated = true;
            // Generate token
            std::string token = generate_token();
            std::string tokenMessage = "Private token: " + token;
            strcpy(cli->token, tokenMessage.c_str());
            // Send token to client
            send(cli->sockfd, cli->token, TOKEN_LENGTH + strlen("private token: "), 0);
        } else {
            send(cli->sockfd, "Invalid username or password.", strlen("Invalid username or password."), 0);
        }
    }

    if (!authenticated) {
        close(cli->sockfd);
        queue_remove(cli->uid);
        delete cli;
        cli_count--;
        pthread_detach(pthread_self());
        return nullptr;
    }

    // Send welcome message with current time
    time_t now = time(0);
    struct tm* currentTime = localtime(&now);
    // sprintf(buff_out + strlen(buff_out), " Welcome %s!\t", username.c_str());
    strftime(buff_out, sizeof(buff_out), "Server:: Welcome!, time: %Y-%m-%d %H:%M:%S\n", currentTime);
    send(cli->sockfd, buff_out, strlen(buff_out), 0);

    sprintf(buff_out, "%s has joined\n", username.c_str());
    std::cout << buff_out;
    send_message(buff_out, cli->uid);

    bzero(buff_out, BUFFER_SZ);

    while (1) {
        // Receive message from the client
        int receive = recv(cli->sockfd, buff_out, BUFFER_SZ, 0);
        if (receive > 0) {
            if (buff_out[13] == '@') {
                // Extract recipient username from the message
                char recipient_name[32];
                int i = 1; // Start after the '@' symbol
                int j = 0; // Index for recipient_name
                while (buff_out[i] != ' ' && buff_out[i] != '\0' && j < 31) {
                    recipient_name[j] = buff_out[i];
                    i++;
                    j++;
                }
                recipient_name[j] = '\0'; // Null-terminate the recipient name

                // Send the message to the specified recipient
                time_t now = time(0);
                struct tm* currentTime = localtime(&now);
                // sprintf(buff_out + strlen(buff_out), " Welcome %s!\t", username.c_str());
                strftime(buff_out, sizeof(buff_out), "Server:: Welcome!, time: %Y-%m-%d %H:%M:%S\n", currentTime);
                send(cli->sockfd, buff_out, strlen(buff_out), 0);
                std::cout<<std::endl;
                send_message_to_user(buff_out, recipient_name);
            } else {
                // Broadcast the message to all clients
                send_message(buff_out, cli->uid);
                std::cout<<std::endl;
                time_t now = time(0);
                struct tm* currentTime = localtime(&now);
                // sprintf(buff_out + strlen(buff_out), " Welcome %s!\t", username.c_str());
                strftime(buff_out, 100, "Server:: Welcome!, time: %Y-%m-%d %H:%M:%S\n", currentTime);
                send(cli->sockfd, buff_out, strlen(buff_out), 0);
            }

            // Handle other parts of message handling...
        }
    }
    /* Delete client from queue and yield thread */
    close(cli->sockfd);
    queue_remove(cli->uid);
    delete cli;
    cli_count--;
    pthread_detach(pthread_self());

    return nullptr;
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <port>" << std::endl;
        return EXIT_FAILURE;
    }

    const char* ip = "127.0.0.1";
    int port = atoi(argv[1]);
    int option = 1;
    int listenfd = 0, connfd = 0;
    struct sockaddr_in serv_addr;
    struct sockaddr_in cli_addr;
    pthread_t tid;

    /* Socket settings */
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip);
    serv_addr.sin_port = htons(port);

    /* Ignore pipe signals */
    signal(SIGPIPE, SIG_IGN);

    if (setsockopt(listenfd, SOL_SOCKET, (SO_REUSEPORT | SO_REUSEADDR), (char*)&option, sizeof(option)) < 0) {
        perror("ERROR: setsockopt failed");
        return EXIT_FAILURE;
    }

    /* Bind */
    if (bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR: Socket binding failed");
        return EXIT_FAILURE;
    }

    /* Listen */
    if (listen(listenfd, 10) < 0) {
        perror("ERROR: Socket listening failed");
        return EXIT_FAILURE;
    }

    std::cout << "=== WELCOME TO THE CHATROOM ===" << std::endl;

    while (1) {
        socklen_t clilen = sizeof(cli_addr);
        connfd = accept(listenfd, (struct sockaddr*)&cli_addr, &clilen);

        /* Check if max clients is reached */
        if ((cli_count + 1) == MAX_CLIENTS) {
            std::cout << "Max clients reached. Rejected: ";
            print_client_addr(cli_addr);
            std::cout << ":" << cli_addr.sin_port << std::endl;
            close(connfd);
            continue;
        }

        /* Client settings */
        client_t* cli = new client_t;
        cli->address = cli_addr;
        cli->sockfd = connfd;
        cli->uid = uid++;

        /* Add client to the queue and fork thread */
        queue_add(cli);
        pthread_create(&tid, nullptr, &handle_client, (void*)cli);

        /* Reduce CPU usage */
        sleep(1);
    }

    return EXIT_SUCCESS;
}
