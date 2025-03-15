#include <iostream>
#include <vector>
#include <unordered_map>
#include <set>
#include <string>
#include <cstring>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

// ==== Visual ====
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"
#define CYAN    "\033[36m"

// === Global ===
#define PORT 59851 // 8080 BUSY on mac
#define MAX_CLIENT 10
#define BUFFER_SIZE 1024

struct ClientInfo {
    int socket;
    string room;
    string name;
};

struct RoomInfo {
    int maxClients = MAX_CLIENT;
    vector<int> clients;
};

vector<ClientInfo> clients;
unordered_map<string, RoomInfo> rooms;

void setNonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

void sendMessage(int clientFd, const string &message) {
    send(clientFd, message.c_str(), message.size(), 0);
}

void sendToRoom(const string &room, int senderFd, const string &message) {
    auto it = rooms.find(room);
    if(it == rooms.end()) return;

    RoomInfo &roomInfo = it->second;
    for (int clientFd : roomInfo.clients) {
        if (clientFd != senderFd) sendMessage(clientFd, message);
    }
}

void setNameClient(ClientInfo &client, string client_name) {
    client_name.erase(remove(client_name.begin(), client_name.end(), '\n'), client_name.end());
    client_name.erase(remove(client_name.begin(), client_name.end(), '\r'), client_name.end());

    if (!client_name.empty()) {
        client.name = client_name;
        sendMessage(client.socket, "Welcome, " + client.name + "!\nUse " YELLOW "/help" RESET " to get page of help.");
    } else {
        sendMessage(client.socket, "Error: Name cannot be empty! Try again.");
    }
}

void commandClientMessage(ClientInfo &client, const string message) {
    stringstream ss(message);
    string command;
    ss >> command;

    if (command == "/create") {
        string nameRoom;
        string maxUsersRoom;
        int maxClients = MAX_CLIENT;

        ss >> nameRoom >> maxUsersRoom;

        if (nameRoom.empty()) {
            sendMessage(client.socket, "Name room is empty! Reapet create room now");
            return;
        }

        if (!maxUsersRoom.empty()) {
            try {
                maxClients = stoi(maxUsersRoom);
            } catch (...) {
                sendMessage(client.socket, "Invalid user limit! Must be a number between 2 and 10.");
                return;
            }
        }

        if (maxClients < 2 || maxClients > 10) {
            sendMessage(client.socket, "User limit must be between 2 and 10.");
            return;
        }

        if (rooms.find(nameRoom) == rooms.end()) {
            rooms[nameRoom] = { maxClients, {client.socket} };
            client.room = nameRoom;
            sendMessage(client.socket, "Room " + nameRoom + " was created successfully!");
        } else {
            sendMessage(client.socket, "A room with that name already exists.");
        }
    } 
    else if (command == "/join") {
        string nameRoom;
        ss >> nameRoom;

        if (nameRoom.empty()) {
            sendMessage(client.socket, "Name room is empty! Reapet create room now");
            return;
        }

        auto it = rooms.find(nameRoom);
        if (it != rooms.end()) {
            RoomInfo &room = it->second;

            if (room.clients.size() >= static_cast<size_t>(room.maxClients)) {
                sendMessage(client.socket, "This room is full! Max users: " + to_string(room.maxClients));
                return;
            }

            client.room = nameRoom;
            room.clients.push_back(client.socket);
            sendMessage(client.socket, "You joined the room '" + nameRoom);
            sendToRoom(nameRoom, client.socket, client.name + " has joined the room!");
        } else {
            sendMessage(client.socket, "This room is not found!");
        }
    } 
    else if (command == "/users") {
        string userList = "List users:\n";
        for (const auto &c : clients) {
            userList += c.name + " " + to_string(c.socket);
            if (!c.room.empty()) {
                userList += " (Room: " + c.room + ")";
            } else {
                userList += " (Not in the room)";
            }

            userList += "\n";
        }
        sendMessage(client.socket, userList);
    } 
    else if (command == "/rooms") {
        string allRoom = "List of rooms:\n";

        int count = 1;
        for (const auto &[roomName, roomInfo] : rooms) {
            allRoom += to_string(count) + ". " + roomName + " (" + 
                    to_string(roomInfo.clients.size()) + "/" + 
                    to_string(roomInfo.maxClients) + " users)\n";
            count++;
        }

        if (rooms.empty()) {
            allRoom += "No active rooms.\n";
        }

        sendMessage(client.socket, allRoom);
    } 
    else if (command == "/leave") {
        if (client.room.empty()) {
            sendMessage(client.socket, "You're not in a room!");
            return;
        }
    
        string nameRoom = client.room;
        auto it = rooms.find(nameRoom);
        
        if (it != rooms.end()) {
            RoomInfo &room = it->second;
            
            room.clients.erase(remove(room.clients.begin(), room.clients.end(), client.socket), room.clients.end());
            
            sendMessage(client.socket, "You left the room: " + nameRoom);
            sendToRoom(nameRoom, client.socket, client.name + " has left the room!");
            
            if (room.clients.empty()) {
                rooms.erase(it);
            }
            
            client.room.clear();
        }
    } 
    else if (command == "/upload") {
        if (client.room.empty()) {
            sendMessage(client.socket, "You're not in a room!");
            return;
        }

        string filename;
        ss >> filename;

        if (filename.empty()) {
            sendMessage(client.socket, "Usage: /upload <filename>");
            return;
        }

        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        getpeername(client.socket, (struct sockaddr*)&client_addr, &addr_len);
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);

        auto it = rooms.find(client.room);
        int num_clients = it->second.clients.size() - 1;

        cout << "Sending START_UPLOAD to " << client.name << ": " << filename << " " << num_clients << endl;
        sendMessage(client.socket, "START_UPLOAD " + filename + " " + to_string(num_clients));

        cout << "Sending PEER_UPLOAD to room " << client.room << ": " << client_ip << " " << filename << endl;
        string upload_msg = "PEER_UPLOAD " + string(client_ip) + " " + filename;
        sendToRoom(client.room, client.socket, upload_msg);
    }
    else if (command == "/help") {
        string allCommand =
            YELLOW "/create" RESET " <name room> <max users>\n"
            YELLOW  "/join" RESET " <name room>\n"
            YELLOW "/users\n" RESET
            YELLOW "/rooms\n" RESET
            YELLOW "/leave\n" RESET
            YELLOW "/upload" RESET " <filename>\n";

        sendMessage(client.socket, allCommand);
    }
    else {
        if (client.room.empty()) sendMessage(client.socket, "You're not in the room!");
        else sendToRoom(client.room, client.socket, client.name + ": " + message);
    }
}

int main() {
    int server_fd, new_socket, max_sd, activity;
    struct sockaddr_in address;
    fd_set readfds;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setNonblocking(server_fd);

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (::bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Error bind()");
        close(server_fd);
        return 1;
    }

    listen(server_fd, MAX_CLIENT);

    // достоверный вывод данных о сервере
    // socklen_t len = sizeof(address);
    // if (getsockname(server_fd, (struct sockaddr*)&address, &len) == -1) {
    //     perror("Ошибка getsockname()");
    // } else {
    //     char ip_str[INET_ADDRSTRLEN];
    //     inet_ntop(AF_INET, &address.sin_addr, ip_str, sizeof(ip_str));
    //     cout << "Сервер запущен на IP: " << ip_str << " и порт: " << ntohs(address.sin_port) << endl;
    // }

    cout << "Server starts on port " << PORT << endl;

    while (true) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (const auto &client : clients) {
            FD_SET(client.socket, &readfds);
            if (client.socket > max_sd) max_sd = client.socket;
        }

        activity = select(max_sd + 1, &readfds, nullptr, nullptr, nullptr);

        if (activity < 0 && errno != EINTR) {
            perror("Error select()");
        }

        if (FD_ISSET(server_fd, &readfds)) {
            socklen_t addr_len = sizeof(address);
            new_socket = accept(server_fd, (struct sockaddr *)&address, &addr_len);
            setNonblocking(new_socket);

            sendMessage(new_socket, "Input your name: ");
            clients.push_back({new_socket, "", ""});
        }

        for (auto it = clients.begin(); it != clients.end(); ) {
            if (FD_ISSET(it->socket, &readfds)) {
                char buffer[BUFFER_SIZE] = {0};
                int valread = read(it->socket, buffer, BUFFER_SIZE);
                if (valread == 0) {
                    close(it->socket);
                    cout << it->name << " disconnected" << endl;
                    it = clients.erase(it);
                } else {
                    buffer[valread] = '\0';
                    string msg = buffer;

                    if (msg.find("P2P_PORT ") == 0) {
                        string port = msg.substr(9);
                        string upload_msg = "PEER_UPLOAD_PORT " + port + "\n";
                        sendToRoom(it->room, it->socket, upload_msg);
                        ++it;
                    }
                    else if (msg.find("FILE_SENT ") == 0) {
                        string result = it->name + ": " + msg.substr(10);
                        sendToRoom(it->room, it->socket, result);
                        sendMessage(it->socket, result);
                        ++it;
                    }
                    else if (msg.find("FILE_RECEIVED ") == 0) {
                        string result = it->name + " received " + msg.substr(14);
                        sendToRoom(it->room, it->socket, result);
                        sendMessage(it->socket, result);
                        ++it;
                    }
                    else if (msg.find("FILE_ERROR") == 0) {
                        string result = it->name + ": File transfer failed - " + msg.substr(11);
                        sendToRoom(it->room, it->socket, result);
                        sendMessage(it->socket, result);
                        ++it;
                    }
                    else if (it->name.empty()) {
                        setNameClient(*it, msg);
                        ++it;
                    } else {
                        commandClientMessage(*it, msg);
                        ++it;
                    }
                }
            } else {
                ++it;
            }
        }

    }

    close(server_fd);
    return 0;
}