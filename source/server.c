#include "server.h"

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv) {
    int sock, bind_res, port;
    struct sockaddr_in addr;
    
    port = atoi(argv[1]);

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port");
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    sock = socket(AF_INET, SOCK_STREAM, 0);

    if (sock == -1) {
        fprintf(stderr, strerror(errno));
        return errno;
    }

    bind_res = bind(sock, &addr, sizeof addr);

    return 0;
}