#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>
#include <string.h>

#include "common.h"

int server_list(SERVER_LIST_RESPONSE *servers) {
  int msgq_id, my_msgq_id;
  msgq_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  if(-1 != msgq_id) {
    SERVER_LIST_REQUEST req;
    req.type = SERVER_LIST;
    req.client_msgid = getpid();
    msgsnd(msgq_id, &req, sizeof(req), 0);
    my_msgq_id = msgget(getpid(), 0666 | IPC_CREAT);
    SERVER_LIST_RESPONSE res;
    msgrcv(my_msgq_id, &res, sizeof(res), SERVER_LIST, 0);
    printf("Active servers: %d\n", res.active_servers);
    int i;
    for(i = 0; i < res.active_servers; i++) {
      printf("Server %d: %d users online\n", res.servers[i], res.clients_on_servers[i]);
    }
    return 0;
  }
  else {
    return -1;
  }
}

void client_release() {
  int msgq_id = msgget(getpid(), 0666);
  msgctl(msgq_id, IPC_RMID, 0);
}

void client_help() {
  printf("LOCALCHAT MANUAL:\n");
  printf("/help to show these instructions\n");
  printf("/servers to list available servers\n");
  printf("/connect <server_id> <login> to login on server\n");
  printf("/exit to quit\n");
}

void client_list_servers() {
  SERVER_LIST_RESPONSE servers;
  server_list(&servers);
}

int server_queue(int server_key) {
  return msgget(server_key, 0666);
}

int client_queue() {
  return msgget(getpid(), 0666 | IPC_CREAT);
}

void client_connect(char *command) {
  int i = 0;
  char server_id[100], client_name[MAX_NAME_SIZE];
  int server_id_size = 0, client_id_size = 0;
  for(i; i < strlen(command); i++) {
    if(' ' != command[i]) break;
  }
  for(i; i < strlen(command); i++) {
    if(' ' == command[i]) break;
    server_id[server_id_size] = command[i];
    server_id_size++;
  }
  server_id[server_id_size] = '\0';
  for(i; i < strlen(command); i++) {
    if(' ' != command[i]) break;
  }
  for(i; i < strlen(command); i++) {
    client_name[client_id_size] = command[i];
    client_id_size++;
  }
  client_name[client_id_size] = '\0';
  if(0 == strlen(server_id) || 0 == strlen(client_name)) {
    printf("Incorrect syntax!\n");
    printf("/connect <server_id> <login> to login on server\n");
  }
  else {
    printf("Connecting to %s as %s...\n", server_id, client_name);
    int server_key = atoi(server_id);
    int server_msgq = server_queue(server_key);
    if(-1 != server_msgq) {
      CLIENT_REQUEST request;
      request.type = LOGIN;
      request.client_msgid = getpid();
      strcpy(request.client_name, client_name);
      msgsnd(server_msgq, &request, sizeof(request), 0);
      STATUS_RESPONSE response;
      msgrcv(client_queue(), &response, sizeof(response), STATUS, 0);
      if(RESPONSE_SERVER_FULL == response.status) {
        printf("Server full!\n");
      }
      else if(RESPONSE_CLIENT_EXISTS == response.status) {
        printf("Client with name %s already exists!\n", client_name);
      }
      printf("received response: %d\n", response.status);
    }
    else {
      printf("Server %s does not exist!\n", server_id);
    }
  }
}

void client_exit() {
  client_release();
  printf("Exiting...\n");
  exit(0);
}

int str_equal(char *a, char *b) {
  if(0 == strcmp(a, b))
    return 1;
  else
    return 0;
}

int str_startswith(char *a, char *b) {
  if(a == strstr(a, b))
    return 1;
  else
    return 0;
}

int main(int argc, char *argv[]) {
  signal(SIGINT, client_release);
  signal(SIGTERM, client_release);

  printf("WELCOME TO LOCALCHAT\n");
  SERVER_LIST_RESPONSE servers;
  int server_status = server_list(&servers);
  if(-1 == server_status) {
    printf("Sorry, no server online...\n");
    printf("Exiting...\n");
    exit(1);
  }

  char *command = malloc(100*sizeof(char));

  while(1) {
    printf("> ");
    gets(command);
    if(str_startswith(command, "/")) {
      if(str_equal(command, "/help")) {
        client_help();
      }
      else if(str_equal(command, "/servers")) {
        client_list_servers();
      }
      else if(str_equal(command, "/exit")) {
        client_exit();
      }
      else if(str_startswith(command, "/connect")) {
        client_connect(command+strlen("/connect"));
      }
      else {
        printf("Unknown command!\n");
        client_help();
      }
    }
    else {
      // send message
    }
  }

  client_release();
  return 0;
}
