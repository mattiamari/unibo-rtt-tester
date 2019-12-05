#ifndef CLIENT_H
#define CLIENT_H

#define _GNU_SOURCE

#include "protocol.h"

#include <netinet/in.h>

#define SEND_BUF_SIZE 1024
#define RECV_BUF_SIZE 33 * 1024
#define SOCK_TIMEOUT_SEC 5


enum client_states {
    STATE_HELLO = 1,
    STATE_MEASURE,
    STATE_BYE,
    STATE_WAIT_BYE_RESP,
    STATE_CLOSE
};

struct client_config {
    struct sockaddr_in server_addr;
    enum measure_types measure_type;
    int n_probes;
    size_t *payload_sizes;
    int n_sizes;
    unsigned int server_delay;
};

void state_hello();
void state_measure();
void state_bye();
void state_wait_bye_resp();
void state_close();

void handle_terminate(int sig);

#endif