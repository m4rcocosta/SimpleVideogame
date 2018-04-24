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

//Add user
int addUser(int ids[], int size, int id2, int* position, int* users_online) {
  if (*users_online == WORLDSIZE) {
    *position = -1;
    return -1;
  }
  int ret = hasUser(ids, WORLDSIZE, id2);
  if (ret != -1) return ret;
  for (int i = 0; i < size; i++) {
    if (ids[i] == -1) {
      ids[i] = id2;
      *users_online += 1;
      *position = i;
      break;
    }
  }
  return -1;
}

// Get ID
int get_ID(int socket) {
  char buf_send[BUFFERSIZE];
  char buf_rcv[BUFFERSIZE];
  IdPacket* request = (IdPacket*)malloc(sizeof(IdPacket));
  PacketHeader ph;
  ph.type = GetId;
  request->header = ph;
  request->id = -1;
  int size = Packet_serialize(buf_send, &(request->header));
  if (size == -1) return -1;
  int bytes_sent = 0;
  int ret = 0;
  while (bytes_sent < size) {
    ret = send(socket, buf_send + bytes_sent, size - bytes_sent, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Can't send ID request.\n");
    if (ret == 0) break;
    bytes_sent += ret;
  }
  Packet_free(&(request->header));
  int ph_len = sizeof(PacketHeader);
  int msg_len = 0;
  while (msg_len < ph_len) {
    ret = recv(socket, buf_rcv + msg_len, ph_len - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(msg_len, "Cannot read from socket.\n");
    msg_len += ret;
  }
  PacketHeader* header = (PacketHeader*)buf_rcv;
  size = header->size - ph_len;

  msg_len = 0;
  while (msg_len < size) {
    ret = recv(socket, buf_rcv + msg_len + ph_len, size - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(msg_len, "Cannot read from socket.\n");
    msg_len += ret;
  }
  IdPacket* deserialized_packet = (IdPacket*)Packet_deserialize(buf_rcv, msg_len + ph_len);
  printf("[Get Id] Received %dbytes \n", msg_len + ph_len);
  int id = deserialized_packet->id;
  Packet_free(&(deserialized_packet->header));
  return id;
}

// Get Elevation Map
Image* get_Elevation_Map(int socket) {
  char buf_send[BUFFERSIZE];
  char buf_rcv[BUFFERSIZE];
  ImagePacket* request = (ImagePacket*)malloc(sizeof(ImagePacket));
  PacketHeader ph;
  ph.type = GetElevation;
  request->header = ph;
  request->id = -1;
  int size = Packet_serialize(buf_send, &(request->header));
  if (size == -1) return NULL;
  int bytes_sent = 0;
  int ret = 0;

  while (bytes_sent < size) {
    ret = send(socket, buf_send + bytes_sent, size - bytes_sent, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Can't send Elevation Map request.\n");
    if (ret == 0) break;
    bytes_sent += ret;
  }

  printf("[Elevation request] Sent %d bytes.\n", bytes_sent);
  int msg_len = 0;
  int ph_len = sizeof(PacketHeader);
  while (msg_len < ph_len) {
    ret = recv(socket, buf_rcv, ph_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Cannot read from socket.\n");
    msg_len += ret;
  }

  PacketHeader* incoming_pckt = (PacketHeader*)buf_rcv;
  size = incoming_pckt->size - ph_len;
  msg_len = 0;
  while (msg_len < size) {
    ret = recv(socket, buf_rcv + msg_len + ph_len, size - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Cannot read from socket.\n");
    msg_len += ret;
  }

  ImagePacket* deserialized_packet = (ImagePacket*)Packet_deserialize(buf_rcv, msg_len + ph_len);
  printf("[Elevation request] Received %d bytes \n", msg_len + ph_len);
  Packet_free(&(request->header));
  Image* elevationMap = deserialized_packet->image;
  free(deserialized_packet);
  return elevationMap;
}

// Get Texture Map
Image* get_Texture_Map(int socket) {
  char buf_send[BUFFERSIZE];
  char buf_rcv[BUFFERSIZE];
  ImagePacket* request = (ImagePacket*)malloc(sizeof(ImagePacket));
  PacketHeader ph;
  ph.type = GetTexture;
  request->header = ph;
  request->id = -1;
  int size = Packet_serialize(buf_send, &(request->header));
  if (size == -1) return NULL;
  int bytes_sent = 0;
  int ret = 0;
  while (bytes_sent < size) {
    ret = send(socket, buf_send + bytes_sent, size - bytes_sent, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Error in send function.\n");
    if (ret == 0) break;
    bytes_sent += ret;
  }
  printf("[Texture request] %d bytes sent.\n", bytes_sent);
  int msg_len = 0;
  int ph_len = sizeof(PacketHeader);
  while (msg_len < ph_len) {
    ret = recv(socket, buf_rcv, ph_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Cannot read from socket.\n");
    msg_len += ret;
  }
  PacketHeader* incoming_pckt = (PacketHeader*)buf_rcv;
  size = incoming_pckt->size - ph_len;
  printf("[Texture Request] Size to read: %d .\n", size);
  msg_len = 0;
  while (msg_len < size) {
    ret = recv(socket, buf_rcv + msg_len + ph_len, size - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Cannot read from socket.\n");
    msg_len += ret;
  }
  ImagePacket* deserialized_packet =
      (ImagePacket*)Packet_deserialize(buf_rcv, msg_len + ph_len);
  printf("[Texture Request] %d bytes received.\n", msg_len + ph_len);
  Packet_free(&(request->header));
  Image* textureMap = deserialized_packet->image;
  free(deserialized_packet);
  return textureMap;
}

// Send Vehicle Texture
int send_Vehicle_Texture(int socket, Image* texture, int id) {
  char buf_send[BUFFERSIZE];
  ImagePacket* request = (ImagePacket*)malloc(sizeof(ImagePacket));
  PacketHeader ph;
  ph.type = PostTexture;
  request->header = ph;
  request->id = id;
  request->image = texture;

  int size = Packet_serialize(buf_send, &(request->header));
  if (size == -1) return -1;
  int bytes_sent = 0;
  int ret = 0;
  while (bytes_sent < size) {
    ret = send(socket, buf_send + bytes_sent, size - bytes_sent, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Can't send vehicle texture.\n");
    if (ret == 0) break;
    bytes_sent += ret;
  }
  printf("[Vehicle texture] %d bytes sent.\n", bytes_sent);
  return 0;
}

// Get Vehicle Texture
Image* get_Vehicle_Texture(int socket, int id) {
  char buf_send[BUFFERSIZE];
  char buf_rcv[BUFFERSIZE];
  ImagePacket* request = (ImagePacket*)malloc(sizeof(ImagePacket));
  PacketHeader ph;
  ph.type = GetTexture;
  request->header = ph;
  request->id = id;
  int size = Packet_serialize(buf_send, &(request->header));
  if (size == -1) return NULL;
  int bytes_sent = 0;
  int ret = 0;
  while (bytes_sent < size) {
    ret = send(socket, buf_send + bytes_sent, size - bytes_sent, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Can't request a texture of a vehicle.\n");
    if (ret == 0) break;
    bytes_sent += ret;
  }
  Packet_free(&(request->header));

  int ph_len = sizeof(PacketHeader);
  int msg_len = 0;
  while (msg_len < ph_len) {
    ret = recv(socket, buf_rcv + msg_len, ph_len - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(msg_len, "Cannot read from socket.\n");
    msg_len += ret;
  }
  PacketHeader* header = (PacketHeader*)buf_rcv;
  size = header->size - ph_len;
  char flag = 0;
  if (header->type == PostDisconnect) flag = 1;
  msg_len = 0;
  while (msg_len < size) {
    ret = recv(socket, buf_rcv + msg_len + ph_len, size - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(msg_len, "Cannot read from socket.\n");
    msg_len += ret;
  }

  if (flag) {
    IdPacket* packet = (IdPacket*)Packet_deserialize(buf_rcv, msg_len + ph_len);
    Packet_free(&packet->header);
    return NULL;
  }
  ImagePacket* deserialized_packet =
      (ImagePacket*)Packet_deserialize(buf_rcv, msg_len + ph_len);
  printf("[Get Vehicle Texture] Received %d bytes.\n", msg_len + ph_len);
  Image* im = deserialized_packet->image;
  free(deserialized_packet);
  return im;
}