#pragma once
#include <netinet/in.h>
#include "vehicle.h"

typedef struct ClientListElement {
  struct ClientListElement* next;
  int id;
  float x, y, theta;
  struct sockaddr_in user_addr;
  Vehicle* vehicle;
  Image* texture;
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
