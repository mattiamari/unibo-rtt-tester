#include "utils.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

void print_recv(const char *msg) {
    printf("<< %s\n", msg);
}

void print_send(const char *msg) {
    printf(">> %s\n", msg);
}
