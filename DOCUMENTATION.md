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
- Just use the `make` command to compile.
- Start Server: digit `./so_game_server ./images/maze.pgm ./images/maze.ppm port_number` (`./so_game_server.c` is the Server executable, `./images/maze.pgm` is the map elevation, `./images/maze.ppm` is the map texture and `port_number` is the port number that is going to be used).
- Start Client: digit `./so_game_client ./images/arrow-right.pgm port_number` (`so_game_client.c` is the Client executable, `./images/arrow-right.pgm` is the texture of the vehicle and `port_number` is the port number that will be used during the connection to the server).