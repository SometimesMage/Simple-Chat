cmake_minimum_required(VERSION 3.0)
project(ChatServer)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Werror")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY  "${CMAKE_CURRENT_SOURCE_DIR}/bin")
set(SERVER_SOURCE_FILES server/main.c server/client.c server/client.h list.c list.h buffer.c buffer.h)
set(CLIENT_SOURCE_FILES client/main.c list.c list.h buffer.c buffer.h client/client.h)

add_executable(ChatServer ${SERVER_SOURCE_FILES})
add_executable(ChatClient ${CLIENT_SOURCE_FILES})