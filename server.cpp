#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unordered_map>
#include <thread>
#include <vector>
#include <random>
#include <mutex>

constexpr int PORT = 8080;
constexpr int MAX_CONNECTIONS = 5;
constexpr int BUFFER_SIZE = 256;

void error(const char *msg) {
	perror(msg);
	exit(EXIT_FAILURE);
}

struct UserInfo {
	std::string username;
	std::string password;
	std::string token;
};

std::unordered_map<std::string, UserInfo> userMap;
std::mutex userMapMutex;

std::string generateToken() {
	std::random_device rd;
	std::mt19937 gen(rd());
	std::uniform_int_distribution<> dis(100000, 999999);
	return std::to_string(dis(gen));
}

// Function to initialize custom users
void initializeUsers() {
	userMap["user1"] = {"user1", "password1", ""};
	userMap["user2"] = {"user2", "password2", ""};
	userMap["user3"] = {"user3", "password3", ""};
	userMap["user4"] = {"user4", "password4", ""};
	userMap["user5"] = {"user5", "password5", ""};
}

void handleClient(int newsockfd) {
	char buffer[BUFFER_SIZE];

	// Authentication
	std::string username, password;
	std::string token;

	std::memset(buffer, 0, BUFFER_SIZE);
	if (write(newsockfd, buffer, 17) < 0)
		error("ERROR writing to socket");
	if (read(newsockfd, buffer, BUFFER_SIZE - 1) < 0)
		error("ERROR reading from socket");
	username = buffer;

	std::memset(buffer, 0, BUFFER_SIZE);
	if (write(newsockfd, buffer, 17) < 0)
		error("ERROR writing to socket");
	if (read(newsockfd, buffer, BUFFER_SIZE - 1) < 0)
		error("ERROR reading from socket");
	password = buffer;

	{
		std::lock_guard<std::mutex> lock(userMapMutex);
		auto it = userMap.find(username);
		if (it == userMap.end() || it->second.password != password) {
			if (write(newsockfd, "Invalid credentials. Closing connection.\n", 42) < 0)
				error("ERROR writing to socket");
			close(newsockfd);
			return;
		}

		token = generateToken();
		it->second.token = token;

		if (write(newsockfd, ("Authentication successful. Token: " + token + "\n").c_str(),
				  ("Authentication successful. Token: " + token + "\n").length()) < 0)
			error("ERROR writing to socket");

		std::cout << "User: " << username << " authenticated with token: " << token << std::endl;
	}

	// Chat loop
	while (true) {
		std::memset(buffer, 0, BUFFER_SIZE);
		int n = read(newsockfd, buffer, BUFFER_SIZE - 1);
		if (n < 0) {
			error("ERROR reading from socket");
			break;
		}

		if (n == 0) {
			std::cout << "Client " << username << " closed the connection.\n";
			break;
		}

		std::cout << username << ": " << buffer << std::endl;

		std::memset(buffer, 0, BUFFER_SIZE);
		std::cout << "Server: ";
		std::cin.getline(buffer, BUFFER_SIZE);

		n = write(newsockfd, buffer, std::strlen(buffer));
		if (n < 0) {
			error("ERROR writing to socket");
			break;
		}
	}

	// Close the socket and remove user information
	close(newsockfd);

	{
		std::lock_guard<std::mutex> lock(userMapMutex);
		userMap.erase(username);
	}

	std::cout << "Connection with user " << username << " closed.\n";
}

int main() {
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

	// Call the function
	initializeUsers();

	clilen = sizeof(cli_addr);

	// Vector to store thread objects
	std::vector<std::thread> threads;

	while (true) {
		newsockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
		if (newsockfd < 0) {
			error("ERROR on accept");
			continue;
		}

		std::cout << "New connection accepted.\n";

		// Create a new thread for each client
		threads.emplace_back(handleClient, newsockfd);
		threads.back().detach();  // Detach the thread
	}

	// Close the main socket
	close(sockfd);

	return 0;
}
