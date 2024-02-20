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
#include <chrono>
#include <ctime>
#include <queue> // Add this line for std::queue
#include <condition_variable>

#include "server.h"

std::atomic<unsigned int> cli_count{0};
int uid = 10;

auto start_time = std::chrono::steady_clock::now();
std::vector<client_t*> clients;
std::mutex clients_mutex;

// Map to store usernames and passwords
std::unordered_map<std::string, std::string> users = {
    {"u1", "p1"},
    {"u2", "p2"},
    {"u3", "p3"},
    {"user4", "password4"},
    {"user5", "password5"}
};



struct MessageData {
    std::string message;
    int timestamp;
    short subscription_flag;
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

void send_welcome_message(int sockfd) {
    char buff_out[BUFFER_SZ];
    time_t now = time(0);
    struct tm* currentTime = localtime(&now);
    strftime(buff_out, sizeof(buff_out), "Server:: Welcome!, time: %Y-%m-%d %H:%M:%S\n", currentTime);
    send(sockfd, buff_out, strlen(buff_out), 0);
}



// Function to send the message back to the sender after a time interval
void send_message_to_self(int sockfd, const char* message, int interval, client_t* client) {

    auto t = make_tuple(message, interval);

    while (client->flags[t].load()) {
        std::this_thread::sleep_for(std::chrono::seconds(interval)); // Wait for the specified interval
        send(sockfd, message, strlen(message), 0); // Send the message back to the sender
        send_welcome_message(sockfd);
    }
}
/*
void unsubscribe_client(client_t* cli) {
    // Send a confirmation message to the client
    const char* confirmation_message = "You have unsubscribed successfully.\n";
    send(sockfd, message, strlen(message), 0);
}
*/
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
    printf("%s", username);
    for (auto& client : clients) {
        if (strcmp(client->name, username) == 0) {
            if (write(client->sockfd, message, strlen(message)) < 0) {
                perror("ERROR: write to descriptor failed");
                break;
            }
        }
    }
}

ssize_t read_exact(int fd, void *buf, size_t count)
{
    ssize_t total = 0;
    ssize_t bytes_read;

    while (total < count) {
        bytes_read = read(fd, (char *) buf + total, count - total);

        if (bytes_read > 0) {
            total += bytes_read;
        } else if (bytes_read == 0) {
            // End of file reached before reading all requested bytes
            break;
        } else {
            perror("read error");
            return -1;
        }
    }

    return total;
}

std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_message_time;

/* Handle all communication with the client */
void* handle_client(void* arg) {
    char buff_out[BUFFER_SZ];
    char name[32];
    int leave_flag = 0;


    cli_count++;
    client_t* cli = static_cast<client_t*>(arg);
    cli->in_loop = true;

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
        memcpy(cli->name, username.c_str(), username.size());

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
        uint32_t sz;
        read_exact(cli->sockfd, (void*)&sz, 4);

        char buff_out[4096];
        auto receive =  read_exact(cli->sockfd, buff_out, sz);

        buff_out[receive] = 0;

        /*      if (receive <= 0) {
            // Client disconnected or encountered an error
            sprintf(buff_out, "%s has left\n", username.c_str());
            std::cout << buff_out;
            send_message(buff_out, cli->uid);
            break; // Exit the loop when client disconnects
        }
*/

        if (receive <= 0) {
            // Client disconnected or encountered an error
            // Ensure username is properly initialized
            std::string username(cli->name); // Assuming cli->name contains the username
            sprintf(buff_out, "%s has left\n", username.c_str());
            std::cout << buff_out;
            send_message(buff_out, cli->uid);
            break; // Exit the loop when client disconnects
        }

        buff_out[receive] = 0;
        printf("%d", sz);



        // Receive message from the client
        if (receive > 0) {
            char* start = buff_out+4;
            char* msg = strchr(start, '@')+1;
            if (msg[0] == '@') {
                // Extract recipient username from the message
                if (strncmp(msg, "@everyone", strlen("@everyone")) == 0) {
                    send_message(buff_out + strlen("@everyone") + 1, cli->uid);
                } else {
                    char recipient_name[32];
                    int i = 1; // Start after the '@' symbol
                    int j = 0; // Index for recipient_name
                    while (msg[i] != ' ' && msg[i] != '\0' && j < 31) {
                        recipient_name[j] = msg[i];
                        i++;
                        j++;
                    }
                    recipient_name[j] = '\0'; // Null-terminate the recipient name

                    send_message_to_user(msg, recipient_name);
                }
            }else {

                std::istringstream iss(msg) ;        //(buff_out)
                std::vector<std::string> parts;
                std::string part;


                while (iss >> part) {
                    parts.push_back(part);
                }

                // Check if the message contains enough parts
                if (parts.size() < 3) {
                    // Invalid message format, skip processing
                    continue;
                }

                MessageData MsgData;
                MsgData.message = parts[0];
                try {
                    MsgData.timestamp = std::stoi(parts[parts.size() - 2]);
                    MsgData.subscription_flag = static_cast<short>(std::stoi(parts[parts.size() - 1]));
                } catch (const std::invalid_argument& e) {
                    // Handle the case where the conversion fails
                    std::cerr << "Invalid argument: " << e.what() << std::endl;
                    continue; // Skip processing this message
                }

                //Testing Logic


                // Extract username from the client struct or use some other identifier
                //	std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_message_time;
                std::string username = cli->name;

                auto current_time = std::chrono::steady_clock::now();

                auto last_message_iter = last_message_time.find(username);
                if (last_message_iter != last_message_time.end() &&
                    current_time - last_message_iter->second <= std::chrono::seconds(2)) {

                } else {

                    last_message_time[username] = current_time;
                }

                auto t = make_tuple(msg, MsgData.timestamp);

                if(MsgData.subscription_flag == 1){
                    // Subscription flag is 1, handle the message normally (broadcast to all clients)
                    send_message(msg, cli->uid);
                    cli->flags[t].store(true);
                    std::thread(send_message_to_self, cli->sockfd, msg, MsgData.timestamp, cli).detach();
                } else {
                    cli->flags[t].store(false);
                }
            }
            send_welcome_message(cli->sockfd);
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
