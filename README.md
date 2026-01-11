# Snake Game

## Architecture

- **Client**: Handles UI, input, and rendering (ncurses)
- **Server**: Manages game logic, collisions, and state
- **Protocol**: Binary message protocol for efficient communication
- **Network**: TCP sockets for reliable IPC

## Project Structure

```
├── src/
│   ├── common/         # Shared code (protocol, network)
│   ├── server/         # Server implementation
│   └── client/         # Client implementation
├── CMakeLists.txt
├── Makefile
└── README.md
```

## Memory Leak Detection

To check for memory leaks using Valgrind:

```bash
# Server
valgrind --leak-check=full ./server

# Client (note: ncurses may show false positives)
valgrind --leak-check=full ./client
```

## Building and Cleaning

```bash
# Build all
make all

# Build server only
make server

# Build client only
make client

# Clean build artifacts
make clean
```

## DATA IMPORT
```bash
# SERVER
scp -P 10022 -r src CMakeLists.txt Makefile README.md example_map.txt sidor@frios2.fri.uniza.sk:~/snake_game/
```



