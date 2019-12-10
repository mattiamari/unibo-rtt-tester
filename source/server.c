#define _POSIX_C_SOURCE 199309L

#include "utils.h"
#include "protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <argp.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define RECV_BUF_SIZE 33*1024
#define MAX_CONNECTIONS 16
#define SOCK_TIMEOUT_SEC 5

enum server_states {
    STATE_HELLO = 1,
    STATE_MEASURE,
    STATE_BYE,
    STATE_CLOSE
};

struct server_config {
    int port;
};

static void state_hello();
static void state_measure();
static void state_bye();
static void state_close();

static void handle_terminate(int sig);
static error_t arg_parser(int key, char *arg, struct argp_state *state);

static char doc[] = "RTT and throughput tester. Server software.";
static char args_doc[] = "PORT";
static struct argp_option options[] = {{0}};
static struct argp argp = {options, arg_parser, args_doc, doc, 0, 0, 0};
static struct server_config config;

static void parse_server_port(const char *arg, struct server_config *config);



static int listen_sock;
static int client_sock;
static char recv_buf[RECV_BUF_SIZE];
static enum server_states current_state;
static msg_hello hello_message;

int main(int argc, char **argv) {
    struct sockaddr_in listen_addr;

    config.port = 0;

    if (argp_parse(&argp, argc, argv, 0, 0, &config) != 0) {
        fprintf(stderr, "Some error occurred while parsing arguments\n");
        exit(1);
    }

    setvbuf(stdout, NULL, _IONBF, 0);

    signal(SIGINT, handle_terminate);

    bzero(&listen_addr, sizeof(struct sockaddr_in));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    listen_addr.sin_port = htons(config.port);

    listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listen_sock == -1) {
        perror("Cannot create socket");
        return errno;
    }

    if (bind(listen_sock, (const struct sockaddr *)&listen_addr, sizeof(listen_addr)) == -1) {
        perror("Cannot bind socket");
        return errno;
    }

    if (listen(listen_sock, MAX_CONNECTIONS)) {
        perror("Cannot listen");
        return errno;
    }

    printf("Listening on port %d\n", config.port);

    current_state = STATE_HELLO;

    while (1) {
        switch (current_state) {
            case STATE_HELLO  : state_hello(); break;
            case STATE_MEASURE: state_measure(); break;
            case STATE_BYE    : state_bye(); break;
            case STATE_CLOSE  : state_close();
        }
    }

    return 0;
}

static void state_hello() {
    struct timeval timeout;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len;
    char addr_str[INET_ADDRSTRLEN];
    ssize_t recv_size;
    const char *response;

    bzero(&recv_buf, RECV_BUF_SIZE);

    printf("Waiting connections\n");
    client_sock = accept(listen_sock, (struct sockaddr *)&client_addr, &client_addr_len);

    // Timeout recv operations after SOCK_TIMEOUT_SEC seconds
    timeout.tv_usec = 0;
    timeout.tv_sec = SOCK_TIMEOUT_SEC;
    if (setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == -1) {
        perror("Cannot set socket options: ");
        current_state = STATE_CLOSE;
        return;
    }
    if (setsockopt(client_sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == -1) {
        perror("Cannot set socket options: ");
        current_state = STATE_CLOSE;
        return;
    }

    inet_ntop(AF_INET, &(client_addr.sin_addr), addr_str, INET_ADDRSTRLEN);
    printf("Client connected: %s on port %d\n", addr_str, client_addr.sin_port);

    recv_size = recv(client_sock, recv_buf, RECV_BUF_SIZE, 0);

    if (recv_size == -1) {
        perror("Receive error");
        current_state = STATE_CLOSE;
        return;
    }

    print_recv(recv_buf);

    if (!hello_from_string(recv_buf, &hello_message)
        || !is_valid_hello(&hello_message)
    ) {
        response = response_strings[RESP_INVALID_HELLO];
        print_send(response);
        send(client_sock, response, strlen(response) + 1, 0);
        current_state = STATE_CLOSE;
        return;
    }

    response = response_strings[RESP_READY];
    print_send(response);
    
    send(client_sock, response, strlen(response) + 1, 0);

    current_state = STATE_MEASURE;
}

static void state_measure() {
    unsigned int expected_seq = 1;
    size_t recv_idx = 0;
    char probe_buf[RECV_BUF_SIZE];
    ssize_t probe_size;
    msg_probe probe;
    const char *response;
    struct timespec delay;

    delay.tv_sec = 0;
    delay.tv_nsec = 0;

    if (hello_message.server_delay > 0) {
        delay.tv_sec = hello_message.server_delay / 1000;

        // the "+1" is needed because nanosleep will not sleep if tv_nsec == 0
        // even when tv_sec is set to some value
        delay.tv_nsec = 1000000 * (hello_message.server_delay % 1000) + 1;
    }

    #ifdef DEBUG
    printf("Artificial delay is %ld seconds and %ld nanoseconds\n", delay.tv_sec, delay.tv_nsec);
    #endif

    while (expected_seq <= hello_message.n_probes) {
        probe_size = recv_until(client_sock, recv_buf, RECV_BUF_SIZE, &recv_idx, probe_buf, RECV_BUF_SIZE, '\n');

        if (probe_size == -1) {
            perror("Receive error");
            current_state = STATE_CLOSE;
            return;
        }

        if (probe_size == -2) {
            fprintf(stderr, "Probe buffer too small\n");
            current_state = STATE_CLOSE;
            return;
        }

        #ifdef DEBUG
        print_recv(probe_buf);
        #endif

        if (!probe_from_string(probe_buf, &probe)
            || !is_valid_probe(&probe, expected_seq)
        ) {
            fprintf(stderr, "Received invalid probe\n");
            response = response_strings[RESP_INVALID_PROBE];
            print_send(response);
            send(client_sock, response, strlen(response) + 1, 0);
            current_state = STATE_CLOSE;
            return;
        }

        printf("Received probe seq %d / %d (%lu bytes)\n",
                probe.probe_seq_num, hello_message.n_probes, probe_size);

        if (delay.tv_nsec > 0) {
            nanosleep(&delay, NULL);
        }

        if (send(client_sock, probe_buf, probe_size, 0) == -1) {
            perror("Probe send error");
            current_state = STATE_CLOSE;
            return;
        }
        expected_seq += 1;
    }

    current_state = STATE_BYE;
}

static void state_bye() {
    msg_bye msg;
    const char *response;

    bzero(recv_buf, RECV_BUF_SIZE);

    if (recv(client_sock, recv_buf, RECV_BUF_SIZE, 0) == -1) {
        perror("Receive error");
        current_state = STATE_CLOSE;
        return;
    }

    print_recv(recv_buf);

    if (!bye_from_string(recv_buf, &msg) || !is_valid_bye(&msg)) {
        fprintf(stderr, "Received invalid Bye message\n");
        current_state = STATE_CLOSE;
        return;
    }

    response = response_strings[RESP_CLOSING];
    print_send(response);
    send(client_sock, response, strlen(response) + 1, 0);

    current_state = STATE_CLOSE;
}

static void state_close() {
    printf("Closing client connection\n");
    close(client_sock);
    current_state = STATE_HELLO;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void handle_terminate(int sig) {
    printf("Interrupt caught. Exiting.\n");
    close(listen_sock);
    close(client_sock);
    exit(0);
}
#pragma GCC diagnostic pop

static error_t arg_parser(int key, char *arg, struct argp_state *state) {
    struct server_config *config = state->input;

    switch (key) {
        case ARGP_KEY_ARG:
            if (state->arg_num >= 1) {
                argp_usage(state);
            }
            parse_server_port(arg, config);
            break;

        case ARGP_KEY_END:
            if (state->arg_num < 1) {
                argp_usage(state);
            }
    }

    return 0;
}

static void parse_server_port(const char *arg, struct server_config *config) {
    config->port = atoi(arg);

    if (config->port < 1 || config->port > 65535) {
        fprintf(stderr, "Invalid port");
        exit(1);
    }
}
