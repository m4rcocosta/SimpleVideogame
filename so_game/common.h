#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

//Configuration parameters
#define TCP_PORT 	              2048
#define UDP_PORT                2801
#define BUFFERSIZE              1000000
#define WORLDSIZE               512
#define LOCALHOST            "127.0.0.1"
#define VEHICLE              "./images/arrow-right.ppm"
#define MAP_ELEVATION        "./images/maze.ppm"
#define MAP_TEXTURE          "./images/maze.pgm"

//Used for debug
#define SERVER	"[SERVER] "
#define CLIENT  "[CLIENT] "
#define TCP		"[TCP] "
#define UDP		"[UDP] "


//Macro for error handling
#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
        if (cond) {                                                 \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)


#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)

#endif
