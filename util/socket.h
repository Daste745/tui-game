#ifndef CLIENT_SOCKET_H
#define CLIENT_SOCKET_H

typedef struct ClientInfo {
    int socket;
    int port;
    pthread_t thread;
} ClientInfo;

int getServerSocket (int port);
int getClientSocket (char* hostname, int port);

#endif //CLIENT_SOCKET_H
