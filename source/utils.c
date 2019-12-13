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
    ssize_t recv_res;

    temp_idx = 0;

    while (temp_idx < temp_size) {
        if (*recv_idx == 0) {
            bzero(recv_buf, recv_size);

            recv_res = recv(fd, recv_buf, recv_size, 0);

            if (recv_res == -1) {
                return -1;
            }

            // Nothing was received, check if connection is still alive
            if (recv_res == 0) {
                if (send(fd, " ", 1, MSG_NOSIGNAL) == -1) {
                    return -1;
                }
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

double get_diff_ms(struct timeval *before, struct timeval *after) {
    return 1000 * (after->tv_sec - before->tv_sec)
    + 0.001 * (after->tv_usec - before->tv_usec);
}

double double_min(double a, double b) {
    return a < b ? a : b;
}

double double_max(double a, double b) {
    return a > b ? a : b;
}
