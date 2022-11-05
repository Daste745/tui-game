#pragma once

#include <pthread.h>

typedef struct ClientInfo {
    int socket;
    int port;
    pthread_t thread;
} ClientInfo;

int getServerSocket(int port);
int getClientSocket(char* hostname, int port);
