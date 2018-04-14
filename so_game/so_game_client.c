
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

void keyPressed(unsigned char key, int x, int y)
{
  switch(key){
  case 27:
    glutDestroyWindow(window);
    exit(0);
  case ' ':
    vehicle->translational_force_update = 0;
    vehicle->rotational_force_update = 0;
    break;
  case '+':
    viewer.zoom *= 1.1f;
    break;
  case '-':
    viewer.zoom /= 1.1f;
    break;
  case '1':
    viewer.view_type = Inside;
    break;
  case '2':
    viewer.view_type = Outside;
    break;
  case '3':
    viewer.view_type = Global;
    break;
  }
}


void specialInput(int key, int x, int y) {
  switch(key){
  case GLUT_KEY_UP:
    vehicle->translational_force_update += 0.1;
    break;
  case GLUT_KEY_DOWN:
    vehicle->translational_force_update -= 0.1;
    break;
  case GLUT_KEY_LEFT:
    vehicle->rotational_force_update += 0.1;
    break;
  case GLUT_KEY_RIGHT:
    vehicle->rotational_force_update -= 0.1;
    break;
  case GLUT_KEY_PAGE_UP:
    viewer.camera_z+=0.1;
    break;
  case GLUT_KEY_PAGE_DOWN:
    viewer.camera_z-=0.1;
    break;
  }
}


void display(void) {
  WorldViewer_draw(&viewer);
}


void reshape(int width, int height) {
  WorldViewer_reshapeViewport(&viewer, width, height);
}

void idle(void) {
  World_update(&world);
  usleep(30000);
  glutPostRedisplay();
  
  // decay the commands
  vehicle->translational_force_update *= 0.999;
  vehicle->rotational_force_update *= 0.7;
}

int main(int argc, char **argv) {
  if (argc<3) {
    printf("usage: %s <port_number> <player texture>\n", argv[1]);
    exit(-1);
  }

  //check port number
  long tmp = strtol(argv[1], NULL, 0);
  if(tmp < 1024 || tmp > 49151) {
    printf("Use a port number between 1024 abd 49151.\n");
    exit(1);
  }

  printf("loading texture image from %s ... ", argv[2]);
  Image* my_texture = Image_load(argv[2]);
  if (my_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  printf("[Client] Starting... \n");
  
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
  struct sockaddr_in server_addr{0};

  server_addr.sin_addr.in_addr = inet_addr(SERVER_IP);
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(argv[1]); //TCP port comes from agrv

  ret = connect(socket_tcp, (struct sockaddr*) &server_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(socket_tcp, "Error while creating socket_tcp");

  //Elevation map request
  IdPacket* elevation_rq = (IdPacket*) malloc(sizeof(IdPacket));
  PacketHeader elevation_header_rq;
  elevation_rq -> header = &elevation_header_rq;
  elevation_rq -> id = my_id;
  elevation_rq -> header -> size = sizeof(IdPacket);
  elevation_rq -> header -> type = GetElevation;
  //to complete

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
