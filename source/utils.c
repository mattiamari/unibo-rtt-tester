#include "utils.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>

void print_recv(const char *msg) {
    printf("<< %s\n", msg);
}

void print_send(const char *msg) {
    printf(">> %s\n", msg);
}

int recv_until(int fd, char *recv_buf, size_t recv_size, size_t *recv_idx, char *temp_buf, size_t temp_size, char sep) {
    size_t temp_idx;

    temp_idx = 0;

    while (temp_idx < temp_size) {
        if (*recv_idx == 0) {
            bzero(recv_buf, recv_size);

            if (recv(fd, recv_buf, recv_size, 0) == -1) {
                return -1;
            }
        }

        if (recv_buf[*recv_idx] != '\0') {
            temp_buf[temp_idx] = recv_buf[*recv_idx];
            temp_idx += 1;
        }
        
        *recv_idx += 1;

        // separator found = message is complete
        if (temp_buf[temp_idx - 1] == sep) {
            return temp_idx;
        }

        // all recv_buf has been read but message is not complete
        if (*recv_idx == recv_size) {
            *recv_idx = 0;
        }
    }

    // temp_buf is full and message is not complete
    return -2;
}

unsigned long get_microseconds(struct timeval *tv) {
    return 1000000 * (*tv).tv_sec + (*tv).tv_usec;
}