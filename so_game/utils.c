#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <semaphore.h>

#include "utils.h"

int receive_tcp(int socket, void *buffer, size_t length, int flags) {
    
    int ret, bytes_read;
    bytes_read = 0;

    if (length > 1) {
        do {
            ret = recv(socket, buffer, length, flags);
        } while (ret == -1 && errno == EINTR);

        if (errno == ENOTCONN) {
            printf("Connection closed.\n");
            return -2;
        }
    }

    else {
        while(1) {
            ret = recv(socket, buffer + bytes_read, length, flags);
            if (errno == EINTR) continue;
            if (errno == ENOTCONN) {
                printf("Connection closed.\n");
                return -2;
            }
            if (buffer[bytes_read] == '\n' || buffer[bytes_read] == '\0') {
                buffer[bytes_read] = '\0';
                bytes_read++;
                break;
            }
            bytes_read++;
        }
        ret = bytes_read;
    }
    return ret;
}