#include <iostream>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define BACKLOG 5
#define BUFFER_SIZE 1024

using namespace std;

int main() {
    int server_fd, new_socket, max_sd, activity;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    char buffer[BUFFER_SIZE];
    fd_set readfds;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = 0;

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    getsockname(server_fd, (struct sockaddr *)&address, &addrlen);
    cout << "Server is running on port " << ntohs(address.sin_port) << endl;

    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen failed");
        exit(EXIT_FAILURE);
    }

    int client_sockets[FD_SETSIZE] = {0};

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (int i = 0; i < FD_SETSIZE; i++) {
            if (client_sockets[i] > 0) {
                FD_SET(client_sockets[i], &readfds);
                if (client_sockets[i] > max_sd) max_sd = client_sockets[i];
            }
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("select error");
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen);
            if (new_socket < 0) {
                perror("accept failed");
                continue;
            }

            cout << "New client connected" << endl;
            for (int i = 0; i < FD_SETSIZE; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    break;
                }
            }
        }

        for (int i = 0; i < FD_SETSIZE; i++) {
            int sd = client_sockets[i];
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                int bytes_read = read(sd, buffer, BUFFER_SIZE - 1);
                if (bytes_read <= 0) {
                    cout << "Client disconnected" << endl;
                    close(sd);
                    client_sockets[i] = 0;
                } else {
                    buffer[bytes_read] = '\0';
                    cout << "Received: " << buffer << endl;
                }
            }
        }
    }
    
    return 0;
}