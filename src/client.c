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
  printf("/exit to quit\n");
}

void client_exit() {
  client_release();
  printf("Exiting...\n");
  exit(0);
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
    scanf("%s", command);
    if(0 == strcmp(command, "/help")) {
      client_help();
    }
    if(0 == strcmp(command, "/exit")) {
      client_exit();
    }
  }

  client_release();
  return 0;
}
