#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>

constexpr int PORT = 8080;
constexpr int MAX_CONNECTIONS = 5;
constexpr int BUFFER_SIZE = 256;

void error(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

struct User {
	std::string username;
	std::string password;
};

std::unordered_map<std::string, User> users;

void initializeUsers() {
	users["user1"] = {"user1", "password1"};
	users["user2"] = {"user2", "password2"};
	users["user3"] = {"user3", "password3"};
	users["user4"] = {"user4", "password4"};
}

bool authenticateUser(const std::string &username, const std::string &password) {
	auto it = users.find(username);
	return it != users.end() && it->second.password == password;
}

int main() {
	initializeUsers();

	int sockfd, newsockfd;
	socklen_t clilen;
	char buffer[BUFFER_SIZE];

	struct sockaddr_in serv_addr, cli_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	std::memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(PORT);

	if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR on binding");

	if (listen(sockfd, MAX_CONNECTIONS) < 0)
		error("ERROR on listen");

	std::cout << "Server listening on port " << PORT << "...\n";

	clilen = sizeof(cli_addr);

	newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
	if (newsockfd < 0)
		error("ERROR on accept");

	std::cout << "Client connected.\n";

	// Authentication
	std::string username, password;
	do {
		std::memset(buffer, 0, BUFFER_SIZE);
		if (write(newsockfd, "Enter username: ", 17) < 0)
			error("ERROR writing to socket");
		if (read(newsockfd, buffer, BUFFER_SIZE - 1) < 0)
			error("ERROR reading from socket");
		username = buffer;

		std::memset(buffer, 0, BUFFER_SIZE);
		if (write(newsockfd, "Enter password: ", 17) < 0)
			error("ERROR writing to socket");
		if (read(newsockfd, buffer, BUFFER_SIZE - 1) < 0)
			error("ERROR reading from socket");
		password = buffer;

		if (!authenticateUser(username, password))
			if (write(newsockfd, "Invalid credentials. Try again.\n", 31) < 0)
				error("ERROR writing to socket");

	} while (!authenticateUser(username, password));

	if (write(newsockfd, "Authentication successful. You can start the chat.\n", 52) < 0)
		error("ERROR writing to socket");

	std::cout << "Authentication successful. You can start the chat.\n";

	// Chat loop
	while (true) {
		std::memset(buffer, 0, BUFFER_SIZE);
		int n = read(newsockfd, buffer, BUFFER_SIZE - 1);
		if (n < 0)
			error("ERROR reading from socket");

		if (n == 0) {
			std::cout << "Client closed the connection.\n";
			break;
		}

		std::cout << "Client (" << username << "): " << buffer << std::endl;

		std::memset(buffer, 0, BUFFER_SIZE);
		std::cout << "Server: ";
		std::cin.getline(buffer, BUFFER_SIZE);

		n = write(newsockfd, buffer, std::strlen(buffer));
		if (n < 0)
			error("ERROR writing to socket");
	}

	if (close(newsockfd) < 0)
		error("ERROR closing socket");

	if (close(sockfd) < 0)
		error("ERROR closing socket");

	return 0;
}
