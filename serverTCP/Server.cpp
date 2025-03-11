#include <iostream>
#include <cstring>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

#define PORT 8080
#define BUFFER_SIZE 1024

using namespace std;

void sigchld_handler(int signo) {
    while (waitpid(-1, nullptr, WNOHANG) > 0);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);
    char buffer[BUFFER_SIZE];

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, nullptr);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    cout << "Server is running and waiting for connections..." << endl;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr*)&address, &addr_len)) < 0) {
            perror("accept failed");
            continue;
        }

        cout << "New connection: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << endl;

        pid_t pid = fork();
        if (pid < 0) {
            perror("fork failed");
            close(new_socket);
        } else if (pid == 0) {
            close(server_fd);

            while (true) {
                int bytes_read = read(new_socket, buffer, BUFFER_SIZE - 1);
                if (bytes_read <= 0) {
                    cout << "Client disconnected: " << inet_ntoa(address.sin_addr) << ":" << ntohs(address.sin_port) << endl;
                    break;
                }

                buffer[bytes_read] = '\0';
                cout << "Received from client: " << buffer << endl;

                int num = atoi(buffer);
                num++;
                snprintf(buffer, BUFFER_SIZE, "%d", num);

                send(new_socket, buffer, strlen(buffer), 0);
                cout << "Sent back to client: " << buffer << endl;
            }

            close(new_socket);
            exit(0);
        } else {
            close(new_socket);
        }
    }

    close(server_fd);
    return 0;
}
