#include "clientList.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void clientList_init(ClientList* head) {
  head->first = NULL;
  head->size = 0;
}

ClientListElement* clientList_find(ClientList* head, int id) {
  if (head == NULL) return NULL;
  ClientListElement* tmp = head->first;
  while (tmp != NULL) {
    if (tmp->id == id)
      return tmp;
    else
      tmp = tmp->next;
  }
  return NULL;
}

ClientListElement* clientList_add(ClientList* head, ClientListElement* elem) {
  if (head == NULL) return NULL;
  ClientListElement* user = head->first;
  elem->next = user;
  head->first = elem;
  head->size++;
  return elem;
}

ClientListElement* clientList_remove(ClientList* head, ClientListElement* elem) {
  if (head == NULL) return NULL;
  ClientListElement* user = head->first;
  if (user == elem) {
    head->first = user->next;
    head->size--;
    return user;
  }
  while (user != NULL) {
    if (user->next == elem) {
      user->next = elem->next;
      head->size--;
      return elem;
    } else
      user = user->next;
  }
  return NULL;
}

void clientList_destroy(ClientList* users) {
  if (users == NULL) return;
  ClientListElement* user = users->first;
  while (user != NULL) {
    ClientListElement* tmp = clientList_remove(users, user);
    user = user->next;
    close(tmp->id);
    free(tmp);
  }
  free(users);
}

void clientList_print(ClientList* users) {
  if (users == NULL) return;
  ClientListElement* user = users->first;
  int i = 0;
  printf("List elements: [");
  while (i < users->size) {
    if (user != NULL) {
      if(i == 0) {
        printf("%d", user->id);
      }
      else printf(",%d", user->id);
      user = user->next;
      i++;
    }
    else break;
  }
  printf("]\n");
}
