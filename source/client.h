#ifndef CLIENT_H
#define CLIENT_H

#define _GNU_SOURCE

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

void print_usage();

void state_hello();
void state_measure();
void state_bye();
void state_wait_bye_resp();
void state_close();

void handle_terminate(int sig);

#endif