
#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>   // struct sockaddr_in
#include <arpa/inet.h>    // htons() and inet_addr()
#include <pthread.h>
#include <sys/socket.h>
#include <signal.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "so_game_protocol.h"
#include "world_viewer.c"
#include "utils.h"
#include "common.h"

// Variables
int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle

int ret, id, socket_tcp, socket_udp;
struct sockaddr_in server_addr_tcp = {0}, server_addr_udp = {0};
char connected = 1;
Image *map_elevation, *map_texture, *my_texture;
localWorld* local_world;

// Clean resources
void clean_resources(void) {
  printf("%sCleaning up...\n", CLIENT);
  for (int i = 0; i < WORLDSIZE; i++) {
    if (local_world->ids[i] == -1) continue;
    if (i == 0) continue;
    local_world->users_online--;
    Image* im = local_world->vehicles[i]->texture;
    World_detachVehicle(&world, local_world->vehicles[i]);
    if (im != NULL) Image_free(im);
    Vehicle_destroy(local_world->vehicles[i]);
  }
  free(local_world->vehicles);
  free(local_world);
  printf("%slocal_world released.\n", CLIENT);
  ret = close(socket_tcp);
  ERROR_HELPER(ret, "Error while closing TCP socket.\n");
  printf("%ssocket_tcp closed.\n", CLIENT);
  ret = close(socket_udp);
  ERROR_HELPER(ret, "Error while closing UDP socket.\n");
  printf("%ssocket_udp closed.\n", CLIENT);
  World_destroy(&world);
  printf("%sworld released\n", CLIENT);
  if(map_elevation == NULL) Image_free(map_elevation);
  if(map_elevation == NULL) Image_free(map_texture);
  if(map_elevation == NULL) Image_free(my_texture);
  printf("%smap_elevation, map_texture, my_texture released\n", CLIENT);
  return;
}

// Handle Signal
void handle_signal(int signal) {
  switch (signal) {
    case SIGHUP:
      break;
    case SIGQUIT:
    case SIGTERM:
    case SIGINT:
      connected = 0;
      sleep(1);
      clean_resources();
      exit(0);
    default:
      fprintf(stderr, "Caught wrong signal: %d\n", signal);
      return;
  }
}

// Send Vehicle Updates
int send_updates(int socket_udp, struct sockaddr_in server_addr, int serverlength) {
  char buf_send[BUFFERSIZE];
  PacketHeader ph;
  ph.type = VehicleUpdate;
  VehicleUpdatePacket* vup = (VehicleUpdatePacket*)malloc(sizeof(VehicleUpdatePacket));
  vup->header = ph;

  // Get Forces Update
  vup->translational_force = vehicle->translational_force_update;
  vup->rotational_force = vehicle->rotational_force_update;

  // Get X, Y, Theta
  vup->x = vehicle->x;
  vup->y = vehicle->y;
  vup->theta = vehicle->theta;

  vup->id = id;
  int size = Packet_serialize(buf_send, &vup->header);
  int bytes_sent = sendto(socket_udp, buf_send, size, 0, (const struct sockaddr*)&server_addr, (socklen_t)serverlength);
  printf("%sSent a VehicleUpdatePacket of %d bytes with tf:%f rf:%f \n", CLIENT, bytes_sent, vup->translational_force, vup->rotational_force);
  Packet_free(&(vup->header));
  if (bytes_sent < 0) return -1;
  return 0;
}

// Send VehicleUpdatePacket to server
void* send_UDP(void* args) {
  client_args udp_args = *(client_args*)args;
  struct sockaddr_in server_addr = udp_args.server_addr_udp;
  int socket_udp = udp_args.socket_udp;
  int server_length = sizeof(server_addr);
  while (connected) {
    int ret = send_updates(socket_udp, server_addr, server_length);
    if (ret == -1)
      printf("%sCannot send VehicleUpdatePacket.\n", CLIENT);
  }
  pthread_exit(NULL);
}

// Receive and apply WorldUpdatePacket from server
void* receive_UDP(void* args) {
  client_args udp_args = *(client_args*)args;
  struct sockaddr_in server_addr = udp_args.server_addr_udp;
  int socket_udp = udp_args.socket_udp;
  socklen_t addrlen = sizeof(server_addr);
  localWorld* lw = udp_args.local_world;
  int socket_tcp = udp_args.socket_tcp;
  while (connected) {
    char receive_buffer[BUFFERSIZE];
    int bytes_read = recvfrom(socket_udp, receive_buffer, BUFFERSIZE, 0, (struct sockaddr*)&server_addr, &addrlen);
    if(bytes_read == -1) {
      printf("%sError while receiving from UDP.\n", CLIENT);
      usleep(500000);
      continue;
    }
    if(bytes_read == 0) {
      usleep(500000);
      continue;
    }
    printf("%sReceived %d bytes from UDP.\n", CLIENT, bytes_read);
    PacketHeader* ph = (PacketHeader*) receive_buffer;
    if(ph->size != bytes_read) ERROR_HELPER(-1, "Error: partial UDP read!\n");
    if(ph->type == WorldUpdate) {
      WorldUpdatePacket* wup = (WorldUpdatePacket*) Packet_deserialize(receive_buffer, bytes_read);
      for(int i = 0; i < wup->num_vehicles; i++) {
        int new_position = -1;
        int id_struct = addUser(lw->ids,WORLDSIZE,wup->updates[i].id, &new_position, &(lw->users_online));
        if(wup->updates[i].id == id) continue;
        else if(id_struct == -1) {
          if(new_position == -1) continue;
          Image* img = get_Vehicle_Texture(socket_tcp, wup->updates[i].id);
          if(img == NULL) continue;
          Vehicle* new_vehicle = (Vehicle*)malloc(sizeof(vehicle));
          Vehicle_init(new_vehicle, &world, wup->updates[i].id, img);
          lw->vehicles[new_position] = new_vehicle;

          // Set Forces Update
          lw->vehicles[new_position]->translational_force = wup->updates[i].translational_force;
          lw->vehicles[new_position]->rotational_force = wup->updates[i].rotational_force;
          // Set X, Y, Theta
          lw->vehicles[new_position]->x = wup->updates[i].x;
          lw->vehicles[new_position]->y = wup->updates[i].y;
          lw->vehicles[new_position]->theta = wup->updates[i].theta;

          World_addVehicle(&world, new_vehicle);
          World_update(&world);
          lw->has_vehicle[new_position] = 1;
        }
        else {
          printf("%sUpdating Vehicle with id %d.\n", CLIENT, wup->updates[i].id);

          // Set X, Y, Theta
          lw->vehicles[id_struct]->x = wup->updates[i].x;
          lw->vehicles[id_struct]->y = wup->updates[i].y;
          lw->vehicles[id_struct]->theta = wup->updates[i].theta;
          // Set Forces Update
          lw->vehicles[id_struct]->translational_force = wup->updates[i].translational_force;
          lw->vehicles[id_struct]->rotational_force = wup->updates[i].rotational_force;
          World_update(&world);

        }
        for(int i = 0; i < WORLDSIZE; i++){
			    int in = 0;
          if (lw->ids[i] == -1) continue;
			    for(int j = 0; j < wup->num_vehicles && !in; j++){
				    if(lw->ids[i] == wup->updates[j].id){
					    in = 1;
				    }
			    }

			    if(!in){
            Image* im = lw->vehicles[i]->texture;
				    World_detachVehicle(&world, lw->vehicles[i]);
            if (im != NULL) Image_free(im);
            Vehicle_destroy(lw->vehicles[i]);
            World_update(&world);
            lw->has_vehicle[i] = -1;
            lw->ids[i] = -1;
            lw->users_online --;
			    }
		    }
      }
    }
    else {
      printf("%sError: received unknown packet.\n", CLIENT);
      connected = 0;
      exit(-1);
    }
  }

  pthread_exit(NULL);
}

int main(int argc, char **argv) {

  if (argc<3) {
    printf("Used: %s %s %s\nUsage: ./so_game_client <server_address> <player texture>\n", argv[0], argv[1], argv[1]==NULL?NULL:argv[2]);
    exit(-1);
  }

  printf("%sLoading texture image from %s ... ", CLIENT, argv[2]);
  my_texture = Image_load(argv[2]);
  if (my_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  printf("%sStarting... \n", CLIENT);

  // Signal handlers
  struct sigaction sa;
  sa.sa_handler = handle_signal;
  // Restart the system call, if at all possible
  sa.sa_flags = SA_RESTART;
  // Block every signal during the handler
  sigfillset(&sa.sa_mask);
  ret = sigaction(SIGHUP, &sa, NULL);
  ERROR_HELPER(ret, "Error: cannot handle SIGHUP.\n");
  ret = sigaction(SIGINT, &sa, NULL);
  ERROR_HELPER(ret, "Error: cannot handle SIGINT.\n");

  // TCP socket
  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_tcp, "Error while creating socket_tcp");

  server_addr_tcp.sin_addr.s_addr = inet_addr(argv[1]);
  server_addr_tcp.sin_family = AF_INET;
  server_addr_tcp.sin_port = htons(TCP_PORT); //TCP port is defined in common.h

  ret = connect(socket_tcp, (struct sockaddr*) &server_addr_tcp, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error while connecting on socket_tcp");

  printf("%sTCP connection established...\n", CLIENT);

  // Setting up localWorld
  local_world = (localWorld*)malloc(sizeof(localWorld));
  local_world->vehicles = (Vehicle**)malloc(sizeof(Vehicle*) * WORLDSIZE);
  for (int i = 0; i < WORLDSIZE; i++) {
    local_world->ids[i] = -1;
    local_world->has_vehicle[i] = 0;
  }

  // Communicating with server
  printf("%sStarting ID,map_elevation,map_texture requests.\n", CLIENT);
  id = get_ID(socket_tcp);
  local_world->ids[0] = id;
  printf("%sID number %d received.\n", TCP, id);
  map_elevation = get_Elevation_Map(socket_tcp, id);
  printf("%sMap elevation received.\n", TCP);
  map_texture = get_Texture_Map(socket_tcp, id);
  printf("%sMap texture received.\n", TCP);
  printf("%sSending vehicle texture...\n", CLIENT);
  send_Vehicle_Texture(socket_tcp, my_texture, id);
  printf("%sClient Vehicle texture sent.\n", TCP);

  // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  vehicle = (Vehicle*)malloc(sizeof(Vehicle));
  Vehicle_init(vehicle, &world, id, my_texture);
  World_addVehicle(&world, vehicle);
  local_world->vehicles[0] = vehicle;
  local_world->has_vehicle[0] = 1;
  printf("%sWorld ready!\n", CLIENT);

  // UDP socket
  socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(socket_udp, "Error while creating socket_udp.\n");
  printf("%sUDP socket created...\n", CLIENT);

  server_addr_udp.sin_addr.s_addr = inet_addr(argv[1]);
  server_addr_udp.sin_family = AF_INET;
  server_addr_udp.sin_port = htons(UDP_PORT);

 // Threads sender and receiver
  pthread_t sender_udp, receiver_udp;
  client_args udp_args;
  udp_args.socket_tcp = socket_tcp;
  udp_args.server_addr_udp = server_addr_udp;
  udp_args.socket_udp = socket_udp;
  udp_args.local_world = local_world;
  ret = pthread_create(&sender_udp, NULL, send_UDP, &udp_args);
  PTHREAD_ERROR_HELPER(ret, "[CLIENT] Error while creating thread sender_udp.\n");
  printf("%sThread sender_udp created.\n", CLIENT);
  ret = pthread_create(&receiver_udp, NULL, receive_UDP, &udp_args);
  PTHREAD_ERROR_HELPER(ret, "[CLIENT] Error while creating thread receiver_udp.\n");
  printf("%sThread receiveer_udp created.\n", CLIENT);

  // Run
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // Waiting threads to end and cleaning resources
  printf("%sDisabling and joining on UDP and TCP threads.\n", CLIENT);
  connected = 0;
  ret = pthread_join(sender_udp, NULL);
  PTHREAD_ERROR_HELPER(ret, "Error pthread_join on thread UDP_sender.\n");
  ret = pthread_join(receiver_udp, NULL);
  PTHREAD_ERROR_HELPER(ret, "Error pthread_join on thread UDP_receiver.\n");
  clean_resources();
  return 0;
}
