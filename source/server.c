#include "server.h"

#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <signal.h>

#define RECV_BUF_SIZE 1024
#define MAX_CONNECTIONS 16

static int sock;

int main(int argc, char **argv) {
    int bind_res;
    int port;
    struct sockaddr_in addr;
    ssize_t recv_size;
    char recv_buf[RECV_BUF_SIZE];

    if (argc < 2) {
        print_usage();
        return 1;
    }

    catch_sigterm();

    port = strtol(argv[1], NULL, 10);

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port");
        return 1;
    }

    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == -1) {
        perror("Cannot create socket");
        return errno;
    }

    bind_res = bind(sock, (const struct sockaddr *)&addr, sizeof(addr));

    if (bind_res == -1) {
        perror("Cannot bind socket");
        return errno;
    }

    if (listen(sock, MAX_CONNECTIONS)) {
        perror("Cannot listen");
        return errno;
    }

    printf("Listening on port %d\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        char addr_str[INET_ADDRSTRLEN];
        int conn = accept(sock, &client_addr, &client_addr_len);

        inet_ntop(AF_INET, &(client_addr.sin_addr), addr_str, INET_ADDRSTRLEN);

        printf("Client connected: %s on port %d\n", addr_str, client_addr.sin_port);

        while (1) {
            bzero(recv_buf, RECV_BUF_SIZE);
            recv_size = recv(conn, recv_buf, RECV_BUF_SIZE, 0);

            if (recv_size == 0) {
                break;
            }

            if (recv_size == -1) {
                perror("Receive error");
                return errno;
            }

            printf("RECV: %s\n", recv_buf);
        }
    }

    return 0;
}

void print_usage() {
    printf("Usage: ./server PORT\n");
}

void catch_sigterm() {
    static struct sigaction sig_act;

    memset(&sig_act, 0, sizeof(sig_act));
    sig_act.sa_sigaction = handle_sigterm;
    sig_act.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &sig_act, NULL);
    sigaction(SIGINT, &sig_act, NULL);
}

void handle_sigterm(int signum, siginfo_t *info, void *ptr) {
    printf("Closing socket\n");
    close(sock);
}