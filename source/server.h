#ifndef SERVER_H
#define SERVER_H

#define _GNU_SOURCE

#include <signal.h>

enum server_states {
    STATE_HELLO = 1,
    STATE_MEASURE,
    STATE_BYE
};

void print_usage();
void catch_sigterm();
void handle_sigterm(int signum, siginfo_t *info, void *ptr);

#endif