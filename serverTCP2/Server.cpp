#include <iostream>
#include <cstring>
#include <cstdlib>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <fstream>

#define PORT 8080
#define BUFFER_SIZE 1024

using namespace std;

pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

struct ClientData {
    int socket;
    sockaddr_in address;
};

void* handle_client(void* arg) {
    ClientData* clientData = (ClientData*)arg;
    int client_socket = clientData->socket;
    sockaddr_in client_address = clientData->address;
    char buffer[BUFFER_SIZE];

    cout << "Client connected: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << endl;

    while (true) {
        int bytes_read = read(client_socket, buffer, BUFFER_SIZE - 1);
        if (bytes_read <= 0) {
            cout << "Client disconnected: " << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << endl;
            break;
        }

        buffer[bytes_read] = '\0';
        cout << "Received from client: " << buffer << endl;

        pthread_mutex_lock(&file_mutex);
        ofstream file("server_log.txt", ios::app);
        if (file.is_open()) {
            file << "Client [" << inet_ntoa(client_address.sin_addr) << ":" << ntohs(client_address.sin_port) << "] sent: " << buffer << endl;
            file.close();
        }
        
        pthread_mutex_unlock(&file_mutex);

        send(client_socket, buffer, strlen(buffer), 0);
    }

    close(client_socket);
    delete clientData;
    return nullptr;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    socklen_t addr_len = sizeof(address);

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
        new_socket = accept(server_fd, (struct sockaddr*)&address, &addr_len);
        if (new_socket < 0) {
            perror("accept failed");
            continue;
        }

        ClientData* clientData = new ClientData{new_socket, address};

        pthread_t thread_id;
        pthread_create(&thread_id, nullptr, handle_client, clientData);
        pthread_detach(thread_id);
    }

    close(server_fd);
    return 0;
}
