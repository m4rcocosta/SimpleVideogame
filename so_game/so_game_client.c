
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


void getter_TCP(Image* my_texture, Image** map_elevation, Image** map_texture, int* my_id){
  char* id_buffer = (char*) calloc(BUFFERSIZE, sizeof(char));
  char* image_packet_buffer = (char*) calloc(BUFFERSIZE, sizeof(char));
  int ret, length, bytes_sent, bytes_read;
  PacketHeader header, im_head;
  
  //ID
  printf("%sID request...\n", CLIENT);
  IdPacket* id_packet = (IdPacket*)calloc(1, sizeof(IdPacket));

  header.type = GetId;
  id_packet -> header = header;
  id_packet -> id = -1;
	
	//Serialize
  length = Packet_serialize(id_buffer, &id_packet->header);
  printf("Bytes written in buffer: %d\n", length);
	Packet_free(&id_packet->header);
	
	//Send buffer
	bytes_sent = 0;
  while(bytes_sent < length){
		ret = send(socket_tcp, id_buffer+bytes_sent, length - bytes_sent, 0);
		if(ret == -1 && errno == EINTR) continue;
		if(ret == 0) break;
		bytes_sent += ret;
    ERROR_HELPER(ret, "Error in send function");
	}

	//Receive buffer
	bytes_read = 0;
  while(bytes_read < length){
		ret = recv(socket_tcp, id_buffer + bytes_read, length - bytes_read, 0);
		if(ret == -1 && errno == EINTR) continue;
    if(ret <= 0) break;
		bytes_read += ret;
		ERROR_HELPER(ret, "Error in recv function");
	}
	
	//Deserialize
  id_packet = (IdPacket*) Packet_deserialize(id_buffer, length);
  *my_id = id_packet -> id;
  printf("%sId found: %d\n", CLIENT, *my_id);


	//Post Texture
	printf("%sPreparing post texture request...\n", CLIENT);
  ImagePacket* image_packet = (ImagePacket*)calloc(1, sizeof(ImagePacket));
  im_head.type = PostTexture;

  image_packet->header = im_head;
  image_packet->image = my_texture;
	
	//Serialize
  length = Packet_serialize(image_packet_buffer, &image_packet->header);
  printf("Bytes written in buffer: %d\n", length);
	Packet_free(&image_packet->header);
		
	//Send buffer
	ret = send(socket_tcp, image_packet_buffer, length, 0);
	free(image_packet_buffer);
	if(DEBUG) printf("%smy_texture: %p\n",CLIENT, my_texture);
	
	
	//Elevation Map
	if(DEBUG) printf("%sPreparing elevation map request\n", CLIENT);
	image_packet_buffer = (char*) calloc(BUFFERSIZE, sizeof(char));
	image_packet = (ImagePacket*) calloc(1, sizeof(ImagePacket));
	
  im_head.type = GetElevation;

  image_packet -> header = im_head;
  image_packet -> id = *my_id;
  image_packet -> image = NULL;
  
  //Serialize
  length = Packet_serialize(image_packet_buffer, &image_packet->header);
  printf("Bytes written in buffer: %d\n", length);
	Packet_free(&image_packet->header);
	
	//Send buffer
	ret = send(socket_tcp, image_packet_buffer,length, 0);
	free(image_packet_buffer);
	
	//Receive buffer
	image_packet_buffer = (char*)calloc(BUFFERSIZE, sizeof(ImagePacket*));
	ret = recv(socket_tcp, image_packet_buffer, BUFFERSIZE, 0);
	
	//Deserialize
	image_packet = (ImagePacket*) Packet_deserialize(image_packet_buffer, ret);
	*map_elevation = image_packet -> image;
	if(DEBUG) printf("%selevation: %p\n",CLIENT, *map_elevation);
	
	
	//Surface Map
	if(DEBUG) printf("%sPreparing surface map request\n", CLIENT);
	image_packet_buffer = (char *) calloc(BUFFERSIZE, sizeof(char));
	image_packet = (ImagePacket*) calloc(1, sizeof(ImagePacket));
	
	im_head.type = GetTexture;
	
	image_packet -> header = im_head;
	image_packet -> id = *my_id;
	image_packet -> image = NULL;
	
	//Serialize
	length = Packet_serialize(image_packet_buffer, &image_packet->header);
	printf("Bytes written in buffer: %d\n", length);
	Packet_free(&image_packet -> header);
	
	//Send buffer
	ret = send(socket_tcp, image_packet_buffer, length, 0);
	free(image_packet_buffer);
	
	//Receive buffer
	ret = recv(socket_tcp, image_packet_buffer, BUFFERSIZE, 0);
	
	//Deserialize
	image_packet = (ImagePacket*) Packet_deserialize(image_packet_buffer, ret);
	*map_texture = image_packet -> image;
	if(DEBUG) printf("%sSurface: %p\n",CLIENT, *map_texture);
}

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
  int my_id, ret;
  char* tcp_port;
  Image* map_elevation;
  Image* map_texture;
  Image* my_texture_from_server;

  my_texture_for_server = Image_load("images/arrow-right.ppm")

  //Connection
  struct sockaddr_in client_addr = {0};

  tcp_port = argv[1];
  socket_tcp = socket(AF_INET, SOCK_STREAM, 0);
  ERROR_HELPER(socket_tcp, "Error while creating socket_tcp.\n");

  client_addr.sin_family = AF_INET;
  client_addr.sin_port = htons(UDP_PORT);
  client_addr.sin_addr.s_addr = inet_addr(tcp_port);

  ret = connect(socket_tcp, (struct sockaddr*) &client_addr, sizeof(struct sockaddr_in));
  ERROR_HELPER(ret, "Error while connecting.\n");
  printf("Connection established.\n");

  //User connection
  getter_TCP(my_texture_for_server, &map_elevation, &map_texture, &my_id);


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

  int socket_udp;

  socket_udp = socket(AF_INET, SOCK_DGRAM, 0);

  pthread_t udp_thread;

  struct args* arg = (struct args*) malloc(sizeof(struct args));
  arg -> idx = my_id;
  arg -> surface_texture = map_texture;
  arg -> vehicle_texture = my_texture;
  arg -> elevation_texture = map_elevation;
  arg -> tcp_socket = socket_tcp;
  arg -> udp_socket = socket_udp;

  ret = pthread_create(&udp_thread, NULL, client_udp_routine, NULL);
  PTHREAD_ERROR_HELPER(ret, "Error while creating udp_thread.\n");

  ret = pthread_detach(udp_thread);

  printf("World runs.1n");
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);
  return 0;             
}
