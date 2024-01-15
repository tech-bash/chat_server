#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<bits/stdc++.h>

constexpr int PORT = 8080;
constexpr int BUFFER_SIZE = 256;

void error(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

int main() {
	int sockfd;
	char buffer[BUFFER_SIZE];

	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0)
		error("ERROR opening socket");

	std::memset((char *)&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(PORT);

	if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0)
		error("ERROR invalid address");

	if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
		error("ERROR connecting");

	// Authentication
	std::string username, password;
	do {
		std::memset(buffer, 0, BUFFER_SIZE);
		if (read(sockfd, buffer, BUFFER_SIZE - 1) < 0)
			error("ERROR reading from socket");
		std::cout << buffer;

		std::cin >> username;
		if (write(sockfd, username.c_str(), username.length()) < 0)
			error("ERROR writing to socket");

		std::memset(buffer, 0, BUFFER_SIZE);
		if (read(sockfd, buffer, BUFFER_SIZE - 1) < 0)
			error("ERROR reading from socket");
		std::cout << buffer;

		std::cin >> password;
		if (write(sockfd, password.c_str(), password.length()) < 0)
			error("ERROR writing to socket");

		std::memset(buffer, 0, BUFFER_SIZE);
		if (read(sockfd, buffer, BUFFER_SIZE - 1) < 0)
			error("ERROR reading from socket");
		std::cout << buffer;

	} while (std::string(buffer) != "Authentication successful. You can start the chat.\n");

	// loop until exit
	while (true) {
		std::cout << "Client: ";
		std::cin >> buffer;

		if (write(sockfd, buffer, strlen(buffer)) < 0)
			error("ERROR writing to socket");

		std::memset(buffer, 0, BUFFER_SIZE);

		int n = read(sockfd, buffer, BUFFER_SIZE - 1);
		if (n < 0)
			error("ERROR reading from socket");

		if (n == 0) {
			std::cout << "Server closed the connection.\n";
			break;
		}

		std::cout << "Server: " << buffer << std::endl;
	}

	if (close(sockfd) < 0)
		error("ERROR closing socket");

	return 0;
}
