cmake_minimum_required(VERSION 3.16)
set(CMAKE_C_STANDARD 11)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread")

project(server C)
add_executable(server server.c util/image.c game/world.c game/entity.h util/api.h util/socket.c util/socket.h util/api.c beast-manager.c)

project(client C)
add_executable(client client.c util/image.c game/world.c game/entity.h game/tile.h util/api.h util/socket.c util/socket.h util/api.c)

project(bot-client C)
add_executable(bot-client bot-client.c util/image.c game/world.c game/entity.h game/tile.h util/api.h util/socket.c util/socket.h util/api.c)
