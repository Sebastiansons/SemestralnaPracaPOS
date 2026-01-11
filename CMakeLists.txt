cmake_minimum_required(VERSION 3.10)
project(SnakeGame C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pthread")

# Find ncurses library
find_package(Curses REQUIRED)
include_directories(${CURSES_INCLUDE_DIR})

# Common sources
set(COMMON_SOURCES
    src/common/protocol.c
    src/common/network.c
)

# Server sources
set(SERVER_SOURCES
    src/server/server.c
    src/server/game_logic.c
    src/server/map.c
    src/server/snake.c
    ${COMMON_SOURCES}
)

# Client sources
set(CLIENT_SOURCES
    src/client/client.c
    src/client/ui.c
    src/client/menu.c
    ${COMMON_SOURCES}
)

# Server executable
add_executable(server ${SERVER_SOURCES})
target_link_libraries(server pthread m)
target_include_directories(server PRIVATE src/common src/server)

# Client executable
add_executable(client ${CLIENT_SOURCES})
target_link_libraries(client ${CURSES_LIBRARIES} pthread m)
target_include_directories(client PRIVATE src/common src/client)

# Custom targets for compatibility
add_custom_target(all_targets ALL DEPENDS server client)
