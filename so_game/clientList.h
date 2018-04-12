#pragma once
#include <netinet/in.h>
#include "vehicle.h"

typedef struct ClientListElement {
  struct ClientListElement* next;
  int id;
  float x, y, theta, prev_x, prev_y, x_shift, y_shift;
  struct sockaddr_in user_addr;
  char inside_world;
  Vehicle* vehicle;
  Image* v_texture;
  float rotational_force, translational_force;
} ClientListElement;

typedef struct ClientList{
  ClientListElement* first;
  int size;
} ClientList;

void clientList_init(ClientList* head);
ClientListElement* clientList_find(ClientList* head, int id);
ClientListElement* clientList_add(ClientList* head, ClientListElement* elem);
ClientListElement* clientList_remove(ClientList* head, ClientListElement* elem);
void clientList_destroy(ClientList* head);
void clientList_print(ClientList* users);
