#include <stdio.h>
#include <stdlib.h>
#include <sys/msg.h>
#include <unistd.h>

#include "common.h"

int server_list(SERVER_LIST_RESPONSE *servers) {
  int msgq_id, my_msgq_id;
  msgq_id = msgget(SERVER_LIST_MSG_KEY, 0666);
  if(-1 != msgq_id) {
    SERVER_LIST_REQUEST req;
    req.type = SERVER_LIST;
    req.client_msgid = getpid();
    msgsnd(msgq_id, &req, sizeof(req), 0);
    my_msgq_id = msgget(getpid(), 0666 | IPC_CREAT | IPC_EXCL);
    SERVER_LIST_RESPONSE res;
    msgrcv(my_msgq_id, &res, sizeof(res), SERVER_LIST, 0);
    printf("Active servers: %d\n", res.active_servers);
    return 0;
  }
  else {
    return -1;
  }
}

int main(int argc, char *argv[]) {
  printf("WELCOME TO LOCALCHAT\n");
  SERVER_LIST_RESPONSE servers;
  int server_status = server_list(&servers);
  if(-1 == server_status) {
    printf("Sorry, no server online...\n");
    printf("Exiting...\n");
    exit(1);
  }
  return 0;
}
