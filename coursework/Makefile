CC = g++
CFLAGS = -Wall -Wextra -std=c++17
LDFLAGS = 

CLIENT_SRC = client.cpp
SERVER_SRC = server.cpp
CLIENT_BIN = client
SERVER_BIN = server

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT_BIN) $(CLIENT_SRC) $(LDFLAGS)

$(SERVER_BIN): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER_BIN) $(SERVER_SRC) $(LDFLAGS)

clean:
	rm -f $(CLIENT_BIN) $(SERVER_BIN)

.PHONY: all clean