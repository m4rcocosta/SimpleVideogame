#include "image.h"
#include <errno.h>


#define SERVER_IP 	"127.0.0.1"
#define UDP_PORT      2801
#define ACTIVE			8
#define BUFFERSIZE	1000000

//USED FOR DEBUG
#define SERVER	"[SERVER] "
#define CLIENT  "[CLIENT] "
#define TCP		"[TCP] "
#define UDP		"[UDP] "



#define GENERIC_ERROR_HELPER(cond, errCode, msg) do {               \
        if (cond) {                                                 \
            fprintf(stderr, "%s: %s\n", msg, strerror(errCode));    \
            exit(EXIT_FAILURE);                                     \
        }                                                           \
    } while(0)


#define ERROR_HELPER(ret, msg)          GENERIC_ERROR_HELPER((ret < 0), errno, msg)
#define PTHREAD_ERROR_HELPER(ret, msg)  GENERIC_ERROR_HELPER((ret != 0), ret, msg)

