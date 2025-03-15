#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <fstream>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace std;

#define BUFFER_SIZE 1024

void sendFile(const string& filename, int server_fd, int main_sockfd, int num_clients) {
    ifstream file(filename, ios::binary);
    if (!file || file.peek() == ifstream::traits_type::eof()) {
        cerr << "Error: Cannot open file or file is empty: " << filename << endl;
        send(main_sockfd, "FILE_ERROR Empty or not found", 28, 0);
        close(server_fd);
        return;
    }

    if (num_clients == 0) {
        send(main_sockfd, "There are no clients to send files to", 38, 0);
        close(server_fd);
        return;
    }

    struct sockaddr_in p2p_addr;
    socklen_t addr_len = sizeof(p2p_addr);
    getsockname(server_fd, (struct sockaddr*)&p2p_addr, &addr_len);
    int p2p_port = ntohs(p2p_addr.sin_port);

    string port_msg = "P2P_PORT " + to_string(p2p_port);
    send(main_sockfd, port_msg.c_str(), port_msg.length(), 0);

    vector<int> client_fds;
    struct timeval timeout;
    timeout.tv_sec = 10;
    timeout.tv_usec = 0;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    for (int i = 0; i < num_clients; ++i) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd >= 0) {
            client_fds.push_back(client_fd);
        } else {
            break;
        }
    }

    if (client_fds.empty()) {
        send(main_sockfd, "FILE_ERROR No clients connected", 31, 0);
        file.close();
        close(server_fd);
        return;
    }

    file.clear();
    file.seekg(0);
    char fileBuffer[BUFFER_SIZE];
    streamsize bytesRead;
    while ((bytesRead = file.read(fileBuffer, BUFFER_SIZE).gcount()) > 0) {
        for (int client_fd : client_fds) {
            send(client_fd, fileBuffer, bytesRead, 0);
        }
    }

    for (int client_fd : client_fds) {
        send(client_fd, "END_FILE", 8, 0);
    }

    int successful_receivers = 0;
    for (int client_fd : client_fds) {
        char buffer[BUFFER_SIZE];
        int recvBytes = recv(client_fd, buffer, BUFFER_SIZE, 0);
        if (recvBytes > 0 && strncmp(buffer, "OK", 2) == 0) {
            successful_receivers++;
        }
        close(client_fd);
    }
    file.close();
    close(server_fd);

    string result_msg = "FILE_SENT " + filename + " " + to_string(successful_receivers) + "/" + to_string(num_clients);
    send(main_sockfd, result_msg.c_str(), result_msg.length(), 0);
}

void receiveFile(const string& peer_ip, int peer_port, const string& filename, int main_sockfd) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation error");
        return;
    }

    struct sockaddr_in peer_addr;
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer_port);
    inet_pton(AF_INET, peer_ip.c_str(), &peer_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) < 0) {
        perror("Connection error");
        close(sockfd);
        return;
    }

    system("mkdir -p downloads");
    ofstream file("downloads/" + filename, ios::binary);
    if (!file) {
        cout << "Error: Cannot save file " << filename << endl;
        close(sockfd);
        return;
    }

    char buffer[BUFFER_SIZE];
    int bytesRead;
    while ((bytesRead = recv(sockfd, buffer, BUFFER_SIZE, 0)) > 0) {
        if (strncmp(buffer, "END_FILE", 8) == 0) break;
        file.write(buffer, bytesRead);
    }
    file.close();

    send(sockfd, "OK", 2, 0);
    close(sockfd);

    string received_msg = "FILE_RECEIVED " + filename;
    send(main_sockfd, received_msg.c_str(), received_msg.length(), 0);
}

void receiveMessages(int sockfd) {
    char buffer[BUFFER_SIZE];
    string peer_ip;
    string filename;
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            cout << "\nDisconnecting from the server...\n";
            close(sockfd);
            exit(0);
        }
        string response = buffer;

        if (response.find("START_UPLOAD ") == 0) {
            string rest = response.substr(13);
            size_t pos = rest.find(" ");
            string filename = rest.substr(0, pos);
            int num_clients = stoi(rest.substr(pos + 1));

            int p2p_fd = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in p2p_addr;
            p2p_addr.sin_family = AF_INET;
            p2p_addr.sin_addr.s_addr = INADDR_ANY;
            p2p_addr.sin_port = 0;

            if (::bind(p2p_fd, (struct sockaddr*)&p2p_addr, sizeof(p2p_addr)) < 0) {
                perror("P2P bind error");
                close(p2p_fd);
                continue;
            }
            listen(p2p_fd, num_clients);
            thread(sendFile, filename, p2p_fd, sockfd, num_clients).detach();
        }
        else if (response.find("PEER_UPLOAD ") == 0) {
            string rest = response.substr(12);
            size_t pos = rest.find(" ");
            peer_ip = rest.substr(0, pos);
            filename = rest.substr(pos + 1);
        }
        else if (response.find("PEER_UPLOAD_PORT ") == 0) {
            int peer_port = stoi(response.substr(17));
            thread(receiveFile, peer_ip, peer_port, filename, sockfd).detach();
        }
        else {
            cout << "\r" << response << endl;
            cout << "> " << flush;
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <port>\n";
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation error");
        return 1;
    }

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &servaddr.sin_addr) <= 0) {
        perror("Invalid server address");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("Connection error");
        close(sockfd);
        return 1;
    }

    cout << "Connected to the server!\n";

    thread receiveThread(receiveMessages, sockfd);
    
    string message;
    cout << "> " << flush;
    while (true) {
        getline(cin, message);
        if (message == "/exit") {
            break;
        }
        if (send(sockfd, message.c_str(), message.length(), 0) == -1) {
            cerr << "Error sending the message!\n";
            break;
        }
        cout << "> " << flush;
    }

    close(sockfd);
    receiveThread.detach();
    return 0;
}