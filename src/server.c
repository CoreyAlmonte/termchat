#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>

#define MAX_EVENTS 2

/**
	server socket created, bound to port. Listens for connections, client socket connects in client.c, 
	server accepts and creates a socket with client info in server.c
	server sends client a response to their connection hello
	client receives and proccesses it
**/


int create_server_socket(uint16_t port) {
	struct sockaddr_in server_info = {0}; 
	server_info.sin_family = AF_INET;
	server_info.sin_port = htons(port);

	socklen_t server_info_len = sizeof(server_info); 
	const int option = 1;
	int optval;
	socklen_t optlen = sizeof(optval);
	
  int server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(server_fd < 0) {
		perror("server socket failed to be created ");
		return -1;
	}
	
  if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,  &option, sizeof(option)) < 0) {
    perror("Failed to set socket opetion SO_REUSEADDR");
    return -1;
  }

  if(bind(server_fd, (const struct sockaddr *)&server_info, server_info_len) < 0) {
		perror("Server socket failed to bind");
		close(server_fd);
		return -1;
	}
	
	if(getsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,  &optval, &optlen) < 0) {
		perror("getsockopt error: REUSEADDR");
		close(server_fd);
		exit(1);
	}

	if(listen(server_fd, 0) < 0) {
		perror("Server socket failed to listen");
		close(server_fd);
		return -1;
	}

  printf("Starting server...\n");
  return server_fd;
}

void handle_client_sockets(unsigned int server_socket) {
	struct sockaddr_in client_info = {0};
	socklen_t client_info_len = sizeof(client_info);
	
	int client_sockets[2] = {0};
	int client_socket_index = 0;
	while(1) {
		client_sockets[client_socket_index] = accept(server_socket, 
			(struct sockaddr *) &client_info, &client_info_len); 
		
		if(client_sockets[client_socket_index] < 0) {
			perror("Server rejects client connection");
			exit(1);
		}

		fcntl(client_sockets[client_socket_index], F_SETFL, O_NONBLOCK);

		char buffer[] = "You are now connected to the server.\n"; 
		if(send(client_sockets[client_socket_index], (void *) buffer, strlen(buffer), 0) < 0) {
			perror("Message to server failed to send");
			exit(1);
		}
    memset(buffer, 0, sizeof(buffer));
    client_socket_index++;	
		printf("client %d of 2\n", client_socket_index);
		if(client_socket_index > 1) {
			break;	
		}
	}
	int client_socket_count = client_socket_index;
	process_clients(client_sockets, client_socket_count);
}
	
void process_clients(int *client_sockets, int client_socket_count) {
	
	struct epoll_event event;
	struct epoll_event events[MAX_EVENTS];
	int epoll_fd = epoll_create1(0);

	for(int i = 0; i < 2; i++) {
		event.data.fd = client_sockets[i];
		event.events = EPOLLIN | EPOLLOUT;
		if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_sockets[i], &event) == -1) {
			perror("epoll_ctl:");
			exit(1);
		}
		// printf("client%d file descripter: %d\n", i, event.data.fd);
		// printf("Is it a correct file descriptor? %s\n\n", (fcntl(event.data.fd, F_GETFL) != -1)  ? "yes" : "no");
  }

	char buffer[100];
  int ready_write = 0;
	int num_fds = 0;
  memset(buffer, 0, sizeof(buffer));
	while(1) {
    num_fds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if(num_fds < 0) {
			perror("Failed epoll_wait: ");
			exit(1);
    }

		for(int i = 0; i < num_fds; i++) { 
      if(events[i].events & EPOLLIN) { 
        recv(events[i].data.fd, buffer, sizeof(buffer), 0);
        printf("received from (%d): %s\n", events[i].data.fd, buffer);
        ready_write = 1;
      }

      if(events[i].events & EPOLLOUT && ready_write == 1) {
        int client_index = 0;
        while(client_index < client_socket_count) {
          if(events[i].data.fd == client_sockets[client_index]) {
            client_index+=1;
            continue;
          }
          send(client_sockets[client_index], (void *) buffer, strlen(buffer), 0);
          printf("sent to (%d): %s\n", client_sockets[client_index], buffer);
          client_index+=1;
        }
        memset(buffer, 0, sizeof(buffer));
        ready_write = 0;
      }
    }
  }
}
