#include <iostream>
#include <thread>
#include <string>
#include <cstring>
#include <fstream>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

using namespace std;

#define BUFFER_SIZE 1024

void receiveMessages(int sockfd) {
    char buffer[BUFFER_SIZE];
    while (true) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(sockfd, buffer, BUFFER_SIZE - 1, 0);
        if (bytes_received <= 0) {
            cout << "\nDisconnecting from the server...\n";
            close(sockfd);
            exit(0);
        }
        string response = buffer;

        if (response.find("READY ") == 0) {
            string filename = response.substr(6);
            ifstream file(filename, ios::binary);
            if (!file) {
                cout << "Error: Cannot open file " << filename << endl;
                send(sockfd, "ERROR", 5, 0);
            } else {
                char fileBuffer[BUFFER_SIZE];
                while (file.read(fileBuffer, BUFFER_SIZE) || file.gcount()) {
                    send(sockfd, fileBuffer, file.gcount(), 0);
                }
                send(sockfd, "END_FILE", 8, 0);  // Завершающее сообщение
                file.close();
                cout << "File " << filename << " sent successfully." << endl;
            }
        } else if (response.find("FILE ") == 0) {
            size_t pos = response.find(" uploaded by ");
            if (pos == string::npos) {
                cerr << "Error: Incorrect file message format!" << endl;
                continue;
            }
            string filename = response.substr(5, pos - 5);

            system("mkdir -p downloads");  // Создаём папку, если её нет
            ofstream file("downloads/" + filename, ios::binary);
            if (!file) {
                cout << "Error: Cannot save file " << filename << endl;
            } else {
                char fileBuffer[BUFFER_SIZE];
                int bytesRead;
                while ((bytesRead = recv(sockfd, fileBuffer, BUFFER_SIZE, 0)) > 0) {
                    if (strncmp(fileBuffer, "END_FILE", 8) == 0) break;
                    file.write(fileBuffer, bytesRead);
                }
                file.close();
                cout << "File " << filename << " received and saved." << endl;
            }
        } else {
            cout << "\r" << response << endl;
        }
        cout << "> " << flush;
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <server_ip> <port>\n";
        return 1;
    }

    int sockfd;
    struct sockaddr_in servaddr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation error");
        return 1;
    }

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