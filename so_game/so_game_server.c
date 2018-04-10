
// #include <GL/glut.h> // not needed here
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include "image.h"
#include "surface.h"
#include "world.h"
#include "vehicle.h"
#include "world_viewer.h"
#include "utils.h"
#include "common.h"

int main(int argc, char **argv) {
  if (argc<4) {
    printf("usage: %s <elevation_image> <texture_image> <port_number>\n", argv[1]);
    exit(-1);
  }
  char* elevation_filename=argv[1];
  char* texture_filename=argv[2];
  long tmp = strtol(argv[3], NULL, 0);		//tcp_port
  if (tmp < 1024 || tmp > 49151) {
	fprintf(stderr, "Use a port number between 1024 and 49151.\n");
	exit(EXIT_FAILURE);
  }
  char* vehicle_texture_filename="./images/arrow-right.ppm";
  printf("loading elevation image from %s ... ", elevation_filename);

  // load the images
  Image* surface_elevation = Image_load(elevation_filename);
  if (surface_elevation) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }


  printf("loading texture image from %s ... ", texture_filename);
  Image* surface_texture = Image_load(texture_filename);
  if (surface_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }

  printf("loading vehicle texture (default) from %s ... ", vehicle_texture_filename);
  Image* vehicle_texture = Image_load(vehicle_texture_filename);
  if (vehicle_texture) {
    printf("Done! \n");
  } else {
    printf("Fail! \n");
  }
  
  int ret, socket_tcp, socket_udp;			//network variables
  struct sockaddr_in server_addr_tcp{0};
  
  if(DEBUG) printf("%s... initializing TCP\n", SERVER);
  if(DEBUG) printf("%s...Socket creation\n", SERVER);
  
  //socket for TCP communication
  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_tcp, "Error in socket creation\n");
  
  server_addr_tcp.sin_family			= AF_INET;
  server_addr_tcp.sin_port				= htons(tmp);
  server_addr_tcp.sin_addr.s_addr	= INADDR_ANY;
  
  int reuseaddr_opt = 1;		//recover a server in case of a crash
  ret = setsockopt(server_tcp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt, sizeof(reuseaddr_opt));
  ERROR_HELPER(ret, "Failed setsockopt() on server socket_tcp");
  
  //binding
  ret = bind(socket_tcp, (struct sockaddr*) &server_addr, sizeof(server_addr);
  ERROR_HELPER(ret, "Error in binding\n");
  
  //listen to a max of 8 connections
  ret = listen(socket_tcp, 8);
  ERROR_HELPER(ret, "Error in listen\n");
  
  if(DEBUG) printf("%s... initializing UDP\n", SERVER);
  if(DEBUG) printf("%s... socket creation\n", SERVER);
  
  //UDP socket
  socket_udp = socket(AF_INET, SOCK_DGRAM, 0);
  ERROR_HELPER(socket_udp, "Error in socket_udp creation\n");
  
  struct sockaddr_in server_addr_udp {0};
  server_addr_udp.sin_family			= AF_INET;
  server_addr_udp.sin_port				= htons(UDP_PORT);
  server_addr_udp.sin_addr.s_addr	= INADDR_ANY;
  
  int reuseaddr_opt_udp = 1;		//recover a server in case of a crash
  ret = setsockopt(socket_udp, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_opt_udp, sizeof(reuseaddr_opt_udp));
  ERROR_HELPER(ret, "Failed setsockopt() on server socket_tcp");
  
  //bind udp
  ret = bind(socket_udp, (struct sockaddr*) &server_addr_udp, sizeof(server_addr_udp));
  ERROR_HELPER(ret, "Error in udp binding\n");
  
  if(DEBUG) printf("%s... UDP socket created\n", SERVER);
  
  if(DEBUG) printf("%s... creating threads for managing communications\n");
  
  thread_server_UDP_args* udp_args = (thread_server_udp_args*)malloc(sizeof(thread_server_udp_args));
  udp_args->socket_desc_udp_server = socket_udp;
  udp_args->connected ////////////

  // not needed here
  //   // construct the world
  // World_init(&world, surface_elevation, surface_texture,  0.5, 0.5, 0.5);

  // // create a vehicle
  // vehicle=(Vehicle*) malloc(sizeof(Vehicle));
  // Vehicle_init(vehicle, &world, 0, vehicle_texture);

  // // add it to the world
  // World_addVehicle(&world, vehicle);


  
  // // initialize GL
  // glutInit(&argc, argv);
  // glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
  // glutCreateWindow("main");

  // // set the callbacks
  // glutDisplayFunc(display);
  // glutIdleFunc(idle);
  // glutSpecialFunc(specialInput);
  // glutKeyboardFunc(keyPressed);
  // glutReshapeFunc(reshape);
  
  // WorldViewer_init(&viewer, &world, vehicle);

  
  // // run the main GL loop
  // glutMainLoop();

  // // check out the images not needed anymore
  // Image_free(vehicle_texture);
  // Image_free(surface_texture);
  // Image_free(surface_elevation);

  // // cleanup
  // World_destroy(&world);
  return 0;             
}
