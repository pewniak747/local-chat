#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>

#include "common.h"

SERVER_LIST_RESPONSE server_list() {
  int msgq_id;
  msgq_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  if(-1 == msgq_id) {
    printf("Sorry, no server online...\n");
    printf("Exiting...\n");
    exit(1);
  }
  else {
    SERVER_LIST_REQUEST req;
    req.type = SERVER_LIST;
    req.client_msgid = getpid();
    msgsnd(msgq_id, &req, sizeof(req), 0);
  }
}

int main(int argc, char *argv[]) {
  printf("WELCOME TO LOCALCHAT\n");
  SERVER_LIST_RESPONSE servers = server_list();
  return 0;
}
