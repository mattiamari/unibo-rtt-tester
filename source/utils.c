#include "utils.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>

void print_recv(char *msg) {
    printf("<< %s\n", msg);
}

void print_send(char *msg) {
    printf(">> %s\n", msg);
}
