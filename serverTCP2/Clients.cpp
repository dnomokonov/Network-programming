#include <iostream>
#include <cstring>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>

#define PORT 8080
#define BUFFER_SIZE 1024

using namespace std;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <number_of_iterations>" << endl;
        return 1;
    }

    int sockfd;
    struct sockaddr_in servaddr;
    char buffer[BUFFER_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
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

    int iterations = atoi(argv[2]);
    for (int i = 1; i <= iterations; i++) {
        snprintf(buffer, BUFFER_SIZE, "%d", i);
        send(sockfd, buffer, strlen(buffer), 0);
        cout << "Sent to server: " << buffer << endl;

        int bytes_received = read(sockfd, buffer, BUFFER_SIZE - 1);
        if (bytes_received <= 0) {
            cerr << "Server disconnected" << endl;
            break;
        }

        buffer[bytes_received] = '\0';
        cout << "Received from server: " << buffer << endl;

        sleep(i);
    }

    close(sockfd);
    return 0;
}
