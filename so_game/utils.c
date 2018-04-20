#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <semaphore.h>

#include "utils.h"

//TCP receive function
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

//TCP send function
int send_tcp(int socket, const void *buffer, size_t length, int flags){
	int bytes_sent = 0, ret;
    while(bytes_sent < length){
		ret = send(tcp_socket_desc, buffer + bytes_sent, length - bytes_sent,0);
		if (ret == -1 && errno == EINTR) continue;
		ERROR_HELPER(ret, "Error in elevation request.\n");
		if (ret == 0) break;
		bytes_sent += ret;
	}
	return bytes_sent;
}

//UDP receive function
int receive_udp(int socket, void *buffer, size_t length, int flags, struct sockaddr *src_addr, socklen_t *addrlen){
	int ret, bytes_read = 0;
	if (length > 1) {
		do {
			ret = recvfrom(socket, buffer, length, flags, src_addr, addrlen);
		} while (ret == -1 && errno == EINTR);
		if (errno == ENOTCONN) {
			printf("Connection closed. ");
			return -2;
		}
	}
	else {
		while (1) {
			ret = recv(socket, buffer + bytes_read, length, flags);
			if (errno == EINTR) continue;
			if (errno == ENOTCONN) {
				printf("Connection closed. ");
				return -2;
			}
			if (buf[bytes_read] == '\n' || buf[bytes_read] == '\0') {
				buf[bytes_read] = '\0';
				bytes_read++;
				break;
			}
			bytes_read++;
		}
		ret = bytes_read;
	}
	return ret;
}

//UDP send function
int send_udp(int socket, const void *buffer, size_t length, int flags, const struct sockaddr *dest_addr, socklen_t addrlen){
	int ret, bytes_sent = 0;
	while (1) {
		ret = sendto(socket, buffer + bytes_sent, length - bytes_sent, flags, dest_addr, addrlen);
		if (errno == EINTR) {
			bytes_sent += ret;
			continue;
		}
		if (errno == ENOTCONN) {
			printf("Connection closed. ");
			return -2;
		}
		bytes_sent += ret;
		if (bytes_sent == length) break;
	}
	return bytes_sent;
}

//Has user
int hasUser(int ids[], int size, int id) {
  for (int i = 0; i < size; i++) {
    if (ids[i] == id) {
      return i;
    }
  }
  return -1;
}
