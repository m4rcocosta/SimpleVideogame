
#include <GL/glut.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/socket.h>
#include <signal.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "so_game_protocol.h"
#include "utils.h"
#include "common.h"

int window;
WorldViewer viewer;
World world;
Vehicle* vehicle; // The vehicle

int ret, my_id;

int main(int argc, char **argv) {

  char *ip_address, *vehicle_texture; //From argv, default LOCALHOST and VEHICLE

  if (argc == 1) {
    ip_address = LOCALHOST;
    vehicle_texture = VEHICLE;
  }

  else if (argc == 2) {
    ip_address = argv[1];
    vehicle_texture = VEHICLE;
  }

  else if (argc == 3) {
    ip_address = argv[1];
    vehicle_texture = argv[2];
  }

  else {
    printf("Error: insert <server_address> (Default:'127.0.0.1') <vehicle_texture> (Default: 'images/arrow-right.ppm')\n");
  }



  printf("loading texture image from %s ... ", vehicle_texture);
  Image* my_texture = Image_load(vehicle_texture);
  if (my_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  printf("%sStarting... \n", CLIENT);
  
  Image* my_texture_for_server;
  // todo: connect to the server
  //   -get ad id
  //   -send your texture to the server (so that all can see you)
  //   -get an elevation map
  //   -get the texture of the surface

  // these come from the server
  Image* map_elevation;
  Image* map_texture;
  Image* my_texture_from_server;

  //My code
  my_texture_for_server = my_texture;

  //TCP socket
  int socket_tcp;
  struct sockaddr_in server_addr_tcp = {0};

  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_tcp, "Error while creating socket_tcp");

  server_addr_tcp.sin_addr.s_addr = inet_addr(ip_address);
  server_addr_tcp.sin_family = AF_INET;
  server_addr_tcp.sin_port = htons(TCP_PORT); //TCP port is defined in common.h

  ret = connect(socket_tcp, (struct sockaddr*) &server_addr_tcp, sizeof(struct sockaddr_in));
  ERROR_HELPER(socket_tcp, "Error while connecting on socket_tcp");

  //UDP socket
  int socket_udp;
  struct sockaddr_in server_addr_udp = {0};

  socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(socket_udp, "Error while creating socket_udp");

  server_addr_udp.sin_addr.s_addr = inet_addr(ip_address);
  server_addr_udp.sin_family = AF_INET;
  server_addr_udp.sin_port = htons(UDP_PORT);

  ret = bind(socket_udp, (struct sockaddr*) &server_addr_udp, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error in bind function on socket_udp");

  //Elevation map request
  IdPacket* elevation_rq = (IdPacket*) malloc(sizeof(IdPacket));
  PacketHeader elevation_header_rq;
  elevation_rq -> header = &elevation_header_rq;
  elevation_rq -> id = my_id;
  elevation_rq -> header -> size = sizeof(IdPacket);
  elevation_rq -> header -> type = GetElevation;

  char elevation_rq_server[BUFFERSIZE];
  char elevation_map[BUFFERSIZE];
  size_t elevation_rq_len = Packet_serialize(elevation_rq_server, &(elevation_rq ->header));

  ret = send_tcp(socket_tcp, elevation_rq_server, elevation_rq_len, 0);
  ERROR_HELPER(ret, "Error while sending my texture for server");

  ret = receive_tcp(socket_tcp, elevation_map, sizeof(ImagePacket), 0);
  ERROR_HELPER(ret, "Error while receiving elevation map from server");

  // construct the world
  World_init(&world, map_elevation, map_texture, 0.5, 0.5, 0.5);
  vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  Vehicle_init(&vehicle, &world, my_id, my_texture_from_server);
  World_addVehicle(&world, v);

  // spawn a thread that will listen the update messages from
  // the server, and sends back the controls
  // the update for yourself are written in the desired_*_force
  // fields of the vehicle variable
  // when the server notifies a new player has joined the game
  // request the texture and add the player to the pool
  /*FILLME*/


  client_args* args = malloc(sizeof(client_args));
  args -> socket_tcp = socket_tcp;
  args -> socket_udp = socket_udp;  
  args -> id = my_id;
  args -> v = vehicle;
  args -> map_texture = map_texture;
  args -> server_addr = server_addr;

  pthread_t thread_tcp, thread_udp;

  ret = pthread_create(&thread_tcp, NULL, handler_tcp, args);
  PTHREAD_ERROR_HELPER(ret, "Error while creating thread_tcp");

  ret = pthread_detach(&thread_tcp);
  PTHREAD_ERROR_HELPER(ret, "Error while detaching thread_tcp");

  ret = pthread_create(&thread_udp, NULL, handler_udp, args);
  PTHREAD_ERROR_HELPER(ret, "Error while creating thread_udp");

  ret = pthread_detach(&thread_udp);
  PTHREAD_ERROR_HELPER(ret, "Error while detaching thread_udp");

  

  printf("World runs.\n");
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);
  return 0;             
}
