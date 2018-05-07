# Documentation

This is a project developed as part of the final exam of the operating systems course.

It is a videogame based on client/server implementation.

## Developers
* [*Marco Costa*](https://github.com/marco-96)
* [*Andrea Antonini*](https://github.com/AndreaAntonini)

## Getting Started

### Prerequisites

This application is written in C.

### How it works
The videogame consists of:
- The client: It is responsible of sending and receiving updates to the server containing infos about the map and the other users and to execute the graphical representation.
- The server: It is supposed to handle the authentication of the new users and to distribute data packet to keep every client updated on what is going on in the map. The server is also responsible of removing inactive users.

### How to run the videogame
- Just use the `make -j` command to compile.
- Start Server: digit `./so_game_server <map_elevation> (Default: "./images/maze.pgm") <map_texture> (Default: "./images/maze.ppm")` (`./so_game_server.c` is the Server executable, `<map_elevation>` is the map elevation and `<map_texture>` is the map texture).
- Start Client: digit `./so_game_client <server_address> (Default: "127.0.0.1") <vehicle_texture> (Default: "images/arrow-right.ppm")` (`./so_game_client` is the Client executable, `<server_address>` is the ip address that will be used during the connection to the server and `vehicle_texture` is the texture of the vehicle).
