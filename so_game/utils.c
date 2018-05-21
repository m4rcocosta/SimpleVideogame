#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <semaphore.h>

#include "utils.h"

// Get ID
int get_ID(int socket) {
  char* buf_send = (char*) malloc(BUFFERSIZE * sizeof(char));;
  char* buf_recv = (char*) malloc(BUFFERSIZE * sizeof(char));;
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
    ret = recv(socket, buf_recv + msg_len, ph_len - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(msg_len, "Cannot read from socket.\n");
    msg_len += ret;
  }
  PacketHeader* header = (PacketHeader*)buf_recv;
  size = header->size - ph_len;

  msg_len = 0;
  while (msg_len < size) {
    ret = recv(socket, buf_recv + msg_len + ph_len, size - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(msg_len, "Cannot read from socket.\n");
    msg_len += ret;
  }
  IdPacket* deserialized_packet = (IdPacket*)Packet_deserialize(buf_recv, msg_len + ph_len);
  if(DEBUG) printf("[Get Id] Received %d bytes, id = %d \n", msg_len + ph_len, deserialized_packet->id);
  int id = deserialized_packet->id;
  free(buf_send);
  free(buf_recv);
  Packet_free(&(deserialized_packet->header));
  return id;
}

// Get Elevation Map
Image* get_Elevation_Map(int socket, int id) {
  char* buf_send = (char*) malloc(BUFFERSIZE * sizeof(char));
  char* buf_recv = (char*) malloc(BUFFERSIZE * sizeof(char));
  ImagePacket* request = (ImagePacket*)malloc(sizeof(ImagePacket));
  PacketHeader ph;
  ph.type = GetElevation;
  request->header = ph;
  request->id = id;
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

  if(DEBUG) printf("[Elevation request] Sent %d bytes.\n", bytes_sent);
  int msg_len = 0;
  int ph_len = sizeof(PacketHeader);
  while (msg_len < ph_len) {
    ret = recv(socket, buf_recv, ph_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Cannot read from socket.\n");
    msg_len += ret;
  }

  PacketHeader* incoming_pckt = (PacketHeader*)buf_recv;
  size = incoming_pckt->size - ph_len;
  msg_len = 0;
  while (msg_len < size) {
    ret = recv(socket, buf_recv + msg_len + ph_len, size - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Cannot read from socket.\n");
    msg_len += ret;
  }

  ImagePacket* deserialized_packet = (ImagePacket*)Packet_deserialize(buf_recv, msg_len + ph_len);
  if(DEBUG) printf("[Elevation request] Received %d bytes \n", msg_len + ph_len);
  Packet_free(&(request->header));
  Image* elevationMap = deserialized_packet->image;
  free(buf_send);
  free(buf_recv);
  free(deserialized_packet);
  return elevationMap;
}

// Get Texture Map
Image* get_Texture_Map(int socket, int id) {
  char* buf_send = (char*) malloc(BUFFERSIZE * sizeof(char));
  char* buf_recv = (char*) malloc(BUFFERSIZE * sizeof(char));
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
    ERROR_HELPER(ret, "Error in send function.\n");
    if (ret == 0) break;
    bytes_sent += ret;
  }
  if(DEBUG) printf("[Texture request] %d bytes sent.\n", bytes_sent);
  int msg_len = 0;
  int ph_len = sizeof(PacketHeader);
  while (msg_len < ph_len) {
    ret = recv(socket, buf_recv, ph_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Cannot read from socket.\n");
    msg_len += ret;
  }
  PacketHeader* incoming_pckt = (PacketHeader*)buf_recv;
  size = incoming_pckt->size - ph_len;
  if(DEBUG) printf("[Texture Request] Size to read: %d .\n", size);
  msg_len = 0;
  while (msg_len < size) {
    ret = recv(socket, buf_recv + msg_len + ph_len, size - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(ret, "Cannot read from socket.\n");
    msg_len += ret;
  }
  ImagePacket* deserialized_packet = (ImagePacket*)Packet_deserialize(buf_recv, msg_len + ph_len);
  if(DEBUG) printf("[Texture Request] %d bytes received.\n", msg_len + ph_len);
  Packet_free(&(request->header));
  Image* textureMap = deserialized_packet->image;
  free(buf_send);
  free(buf_recv);
  free(deserialized_packet);
  return textureMap;
}

// Send Vehicle Texture
int send_Vehicle_Texture(int socket, Image* texture, int id) {
  char* buf_send = (char*) malloc(BUFFERSIZE * sizeof(char));
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
  if(DEBUG) printf("[Vehicle texture] %d bytes sent.\n", bytes_sent);
  free(buf_send);
  return 0;
}

// Get Vehicle Texture
Image* get_Vehicle_Texture(int socket, int id) {
  char* buf_send = (char*) malloc(BUFFERSIZE * sizeof(char));
  char* buf_recv = (char*) malloc(BUFFERSIZE * sizeof(char));
  ImagePacket* request = (ImagePacket*)malloc(sizeof(ImagePacket));
  PacketHeader ph;
  ph.type = GetVehicleTexture;
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
    ret = recv(socket, buf_recv + msg_len, ph_len - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(msg_len, "Cannot read from socket.\n");
    msg_len += ret;
  }
  PacketHeader* header = (PacketHeader*)buf_recv;
  size = header->size - ph_len;
  msg_len = 0;
  while (msg_len < size) {
    ret = recv(socket, buf_recv + msg_len + ph_len, size - msg_len, 0);
    if (ret == -1 && errno == EINTR) continue;
    ERROR_HELPER(msg_len, "Cannot read from socket.\n");
    msg_len += ret;
  }
  ImagePacket* deserialized_packet = (ImagePacket*)Packet_deserialize(buf_recv, msg_len + ph_len);
  if(DEBUG) printf("[Get Vehicle Texture] Received %d bytes.\n", msg_len + ph_len);
  Image* im = deserialized_packet->image;
  free(buf_send);
  free(buf_recv);
  free(deserialized_packet);
  return im;
}
