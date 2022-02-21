#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include "socket.h"


int getServerSocket (int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return -1;
    }

    struct sockaddr_in server = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr.s_addr = INADDR_ANY
    };

    if (bind(sock, (struct sockaddr *) &server, sizeof(server)) == -1)
        return -2;

    listen(sock, 5);

    return sock;
}

int getClientSocket (char* hostname, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        return -1;
    }

    struct hostent *server_host = gethostbyname(hostname);
    if (server_host == NULL) {
        return -2;
    }

    struct sockaddr_in server = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
    };
    bcopy((char *) server_host->h_addr_list[0],
          (char *) &server.sin_addr.s_addr,
          server_host->h_length);

    if(connect(sock, (struct sockaddr *) &server, sizeof(server)) == -1) {
        return -3;
    }

    return sock;
}
