#ifndef UTILS_H
#define UTILS_H

#include <stdlib.h>
#include <sys/time.h>

void print_recv(const char *msg);
void print_send(const char *msg);

int recv_until(int fd,
               char *recv_buf,
               size_t recv_size,
               size_t *recv_idx,
               char *temp_buf,
               size_t temp_size,
               char sep);

unsigned long get_diff_ms(struct timeval *before, struct timeval *after);

unsigned long ulmin(unsigned long a, unsigned long b);
unsigned long ulmax(unsigned long a, unsigned long b);

#endif
