#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <server_ip> <port> <i>" << std::endl;
        return 1;
    }

    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[BUFFER_SIZE];

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
        perror("invalid address");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("connection failed");
        close(sockfd);
        return 1;
    }

    int delay = atoi(argv[3]);
    while (true) {
        snprintf(buffer, BUFFER_SIZE, "%d", delay);
        send(sockfd, buffer, strlen(buffer), 0);
        std::cout << "Sent: " << buffer << std::endl;
        sleep(delay);
    }

    close(sockfd);
    return 0;
}