
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
#include <time.h>
#include <fcntl.h>

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

int my_id;
int soclet_tcp; //socket tcp
int server_connected;
playerWorld player_world;
sem_t* world_update_sem;

void* Sender_UDP(void* args) {
  udpArgs udp_args = (udpArgs*) args;
  struct sockaddr_in server_addr = udp_args.server_addr;
  int socket_udp = udp_args.socket_udp;
  int server_len = sizeof(server_addr);
  while(server_connected) {
    ret = send_updates(socket_udp, server_addr, server_len);
    PTHREAD_ERROR_HELPER(ret, "Error in send_updates function");
  }
  close(s);
  return 0;
}

void* Receiver_UDP(void* args) {
  udpArgs udp_args = (udpArgs*) args;
  struct sockaddr_in server_addr = udp_args.server_addr;
  int socket_udp = udp_args.socket_udp;
  int server_len = sizeof(server_addr);
  int bytes_read;
  char receive_buffer[BUFFERSIZE];
  while(server_connected) {
    bytes_read = recvfrom(socket_udp, receive_buffer, BUFFERSIZE, 0, (struct sockaddr*) &server_addr, server_len);
    PTHREAD_ERROR_HELPER(bytes_read, "Error in recvfrom function.\n");
    ret = packet_analysis(receive_buffer, bytes_read);
    PTHREAD_ERROR_HELPER(ret, "Error in packet_analysis function.\n");
  }
}

int packet_analisys(char* buffer, int len) {	
	World world_aux = world;
	ret=sem_wait(world_update_sem);
	PTHREAD_ERROR_HELPER(ret,"Error in sem_wait function in world_updater.\n");
	int i, new_players=0;
  WorldUpdatePacket* deserialized_wu_packet = (WorldUpdatePacket*) Packet_deserialize(world_buffer, world_buffer_size);
	if(deserialized_wu_packet -> header -> type != WorldUpdate) return 0;
	// serve?? if(deserialized_wu_packet->header->size != len) return 0; perchè l'udp non ha trasportato tutti i dati?
	ClientUpdate* aux = deserialized_wu_packet -> updates;
	while(aux != NULL){
		for(int i=0; i < WORLDSIZE; i++){
			if (player_world -> id_list[i] == aux -> id){
				Vehicle_setXYTheta(player_world -> vehicles[i], deserialized_wu_packet -> updates -> x, deserialized_wu_packet -> updates -> y, deserialized_wu_packet -> updates -> theta);
				break;
			}
		new_players=1;
		}
	}
	if (deserialized_wu_packet -> num_vehicles != player_world -> players_online || new_players) check_newplayers(deserialized_wu_packet); //Add new players
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
  Image* map_elevation;
  Image* map_texture;
  Image* my_texture_from_server;

  my_texture_for_server;


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
  

  printf("World runs.\n");
  WorldViewer_runGlobal(&world, vehicle, &argc, argv);

  // cleanup
  World_destroy(&world);
  return 0;             
}
