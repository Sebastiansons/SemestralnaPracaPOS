# Makefile for Snake Game
CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -D_DEFAULT_SOURCE -pthread -I src/common -I src/server -I src/client
LDFLAGS = -pthread -lm

# Directories
BUILD_DIR = build
SRC_DIR = src
COMMON_DIR = $(SRC_DIR)/common
SERVER_DIR = $(SRC_DIR)/server
CLIENT_DIR = $(SRC_DIR)/client

# Common sources
COMMON_SOURCES = $(COMMON_DIR)/protocol.c $(COMMON_DIR)/network.c
COMMON_OBJECTS = $(BUILD_DIR)/protocol.o $(BUILD_DIR)/network.o

# Server sources
SERVER_SOURCES = $(SERVER_DIR)/server.c $(SERVER_DIR)/game_logic.c $(SERVER_DIR)/map.c $(SERVER_DIR)/snake.c
SERVER_OBJECTS = $(BUILD_DIR)/server.o $(BUILD_DIR)/game_logic.o $(BUILD_DIR)/map.o $(BUILD_DIR)/snake.o

# Client sources
CLIENT_SOURCES = $(CLIENT_DIR)/client.c $(CLIENT_DIR)/ui.c $(CLIENT_DIR)/menu.c
CLIENT_OBJECTS = $(BUILD_DIR)/client.o $(BUILD_DIR)/ui.o $(BUILD_DIR)/menu.o

# Targets
.PHONY: all server client clean

all: server client

server: $(BUILD_DIR) $(COMMON_OBJECTS) $(SERVER_OBJECTS)
	$(CC) $(COMMON_OBJECTS) $(SERVER_OBJECTS) -o server $(LDFLAGS)

client: $(BUILD_DIR) $(COMMON_OBJECTS) $(CLIENT_OBJECTS)
	$(CC) $(COMMON_OBJECTS) $(CLIENT_OBJECTS) -o client $(LDFLAGS) -lncurses

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Common objects
$(BUILD_DIR)/protocol.o: $(COMMON_DIR)/protocol.c $(COMMON_DIR)/protocol.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/network.o: $(COMMON_DIR)/network.c $(COMMON_DIR)/network.h
	$(CC) $(CFLAGS) -c $< -o $@

# Server objects
$(BUILD_DIR)/server.o: $(SERVER_DIR)/server.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/game_logic.o: $(SERVER_DIR)/game_logic.c $(SERVER_DIR)/game_logic.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/map.o: $(SERVER_DIR)/map.c $(SERVER_DIR)/map.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/snake.o: $(SERVER_DIR)/snake.c $(SERVER_DIR)/snake.h
	$(CC) $(CFLAGS) -c $< -o $@

# Client objects
$(BUILD_DIR)/client.o: $(CLIENT_DIR)/client.c
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ui.o: $(CLIENT_DIR)/ui.c $(CLIENT_DIR)/ui.h
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/menu.o: $(CLIENT_DIR)/menu.c $(CLIENT_DIR)/menu.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR) server client
