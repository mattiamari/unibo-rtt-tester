#include "client.h"

#include "utils.h"
#include "protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <strings.h>
#include <string.h>
#include <argp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

static error_t arg_parser(int key, char *arg, struct argp_state *state);
static void parse_measure_type(const char *arg, struct client_config *config);
static void parse_probe_num(const char *arg, struct client_config *config);
static void parse_payload_size(const char *arg, struct client_config *config);
static void parse_server_addr(const char *arg, struct client_config *config);
static void parse_server_port(const char *arg, struct client_config *config);

static char doc[] = "RTT and throughput tester";
static char args_doc[] = "SERVER_ADDR PORT";

static struct argp_option options[] = {
    {"measure", 'm', "TYPE", 0, "Type of measure to perform (rtt | thput). Defaults to 'rtt'."},
    {"n_probes", 'n', "NUM", 0, "Number of probes to send, Defaults to 20."},
    {"size", 's', "BYTES", 0, "Size of the probe's payload."},
    {0}
};

static struct argp argp = {options, arg_parser, args_doc, doc};

static unsigned short current_state;
static int sock;
static char recv_buf[RECV_BUF_SIZE];
static struct client_config config;
static msg_hello hello_message;



int main(int argc, char **argv) {
    struct timeval timeout;
    char addr_str[INET_ADDRSTRLEN];

    config.measure_type = MEASURE_RTT;
    config.n_probes = 20;
    config.payload_sizes = default_msg_size_rtt;
    config.n_sizes = sizeof default_msg_size_rtt / sizeof default_msg_size_rtt[0];
    config.server_delay = 0;
    bzero(&(config.server_addr), sizeof(struct sockaddr_in));
    config.server_addr.sin_family = AF_INET;

    if (argp_parse(&argp, argc, argv, 0, 0, &config) != 0) {
        fprintf(stderr, "Some error occurred while parsing arguments\n");
        exit(1);
    }

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (sock == -1) {
        perror("Cannot create socket");
        return errno;
    }

    // Timeout recv operations after SOCK_TIMEOUT_SEC seconds
    timeout.tv_usec = 0;
    timeout.tv_sec = SOCK_TIMEOUT_SEC;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    signal(SIGTERM, handle_terminate);

    if (connect(sock, &(config.server_addr), sizeof(config.server_addr)) == -1) {
        perror("Cannot connect host");
        return errno;
    }

    inet_ntop(AF_INET, &(config.server_addr.sin_addr), addr_str, INET_ADDRSTRLEN);
    printf("Connected to %s on port %d\n", addr_str, config.server_addr.sin_port);

    current_state = STATE_HELLO;

    while(1) {
        switch (current_state) {
            case STATE_HELLO: state_hello(); break;
            case STATE_MEASURE: state_measure(); break;
            case STATE_BYE: state_bye(); break;
            case STATE_WAIT_BYE_RESP: state_wait_bye_resp(); break;
            case STATE_CLOSE: state_close();
        }
    }

    return 0;
}

void state_hello() {
    size_t msg_str_len;
    char msg_str[MAX_SIZE_HELLO];

    hello_message.protocol_phase = PHASE_HELLO;
    hello_message.measure_type = config.measure_type;
    hello_message.n_probes = config.n_probes;
    hello_message.msg_size = config.payload_sizes[0];
    hello_message.server_delay = config.server_delay;

    if (!hello_to_string(&hello_message, msg_str, &msg_str_len)) {
        fprintf(stderr, "Cannot serialize Hello message");
        current_state = STATE_CLOSE;
        return;
    }

    printf("Sending hello message. (%lu bytes)\n", msg_str_len);
    print_send(msg_str);
    send(sock, msg_str, msg_str_len, 0);

    recv(sock, recv_buf, RECV_BUF_SIZE, 0);
    print_recv(recv_buf);

    if (response_is(recv_buf, RESP_READY)) {
        current_state = STATE_MEASURE;
    } else {
        current_state = STATE_MEASURE;
    }
}

void state_measure() {
    char *payload;
    msg_probe probe;
    char probe_str[MAX_SIZE_PROBE];
    size_t probe_str_len;

    printf("Starting measure\n");

    payload = new_payload(hello_message.msg_size);
    probe.protocol_phase = PHASE_MEASURE;
    probe.payload = payload;

    for (int i = 1; i <= hello_message.n_probes; i++) {
        probe.probe_seq_num = i;

        if (!probe_to_string(&probe, probe_str, &probe_str_len)) {
            fprintf(stderr, "Cannot serialize probe");
            free(payload);
            current_state = STATE_CLOSE;
            return;
        }
        
        send(sock, probe_str, probe_str_len, 0);
    }

    free(payload);
    current_state = STATE_BYE;
}

void state_bye() {
    msg_bye bye;
    char bye_str[MAX_SIZE_BYE];
    size_t bye_str_len;

    printf("Sending bye message\n");

    bye.protocol_phase = PHASE_BYE;
    bye_to_string(&bye, bye_str, &bye_str_len);

    print_send(bye_str);
    send(sock, bye_str, bye_str_len, 0);

    current_state = STATE_WAIT_BYE_RESP;
}

void state_wait_bye_resp() {
    int recv_res;

    printf("Waiting bye response\n");

    recv_res = recv(sock, recv_buf, RECV_BUF_SIZE, 0);

    if (recv_res == -1) {
        perror("Receive error");
        current_state = STATE_CLOSE;
        return;
    }

    if (recv_res > 0) {
        print_recv(recv_buf);
    } else if (recv_res == 0) {
        printf(".");
    }

    if (response_is(recv_buf, RESP_CLOSING)) {
        current_state = STATE_CLOSE;
    }
}

void state_close() {
    printf("Closing\n");
    close(sock);
    exit(EXIT_SUCCESS);
}

void handle_terminate(int sig) {
    printf("Caught SIGINT. Closing.\n");
    close(sock);
}

static error_t arg_parser(int key, char *arg, struct argp_state *state) {
    struct client_config *config = state->input;

    switch (key) {
        case 'm': parse_measure_type(arg, config); break; 
        case 'n': parse_probe_num(arg, config); break;
        case 's': parse_payload_size(arg, config); break;
        
        case ARGP_KEY_ARG:
            if (state->arg_num >= 2) {
                argp_usage(state);
            }
            switch (state->arg_num) {
                case 0: parse_server_addr(arg, config); break;
                case 1: parse_server_port(arg, config);
            }
            break;
        
        case ARGP_KEY_END:
            if (state->arg_num < 2) {
                argp_usage(state);
            }
    }

    return 0;
}

static void parse_measure_type(const char *arg, struct client_config *config) {
    if (strcmp("rtt", arg) == 0) {
        config->measure_type = MEASURE_RTT;
    } else if (strcmp("thput", arg) == 0) {
        config->measure_type = MEASURE_THPUT;
    } else {
        fprintf(stderr, "Invalid measure type");
        exit(1);
    }
}

static void parse_probe_num(const char *arg, struct client_config *config) {
    config->n_probes = atoi(arg);
    if (config->n_probes < 1) {
        fprintf(stderr, "Invalid n_probes");
        exit(1);
    }
}

static void parse_payload_size(const char *arg, struct client_config *config) {
    size_t *payload_size = malloc(sizeof(size_t));
    *payload_size = atol(arg);

    if (*payload_size < 1 || *payload_size > 32 K) {
        fprintf(stderr, "Invalid payload size");
        exit(1);
    }

    config->payload_sizes = payload_size;
    config->n_sizes = 1;
}

static void parse_server_addr(const char *arg, struct client_config *config) {
    if (inet_aton(arg, &(config->server_addr.sin_addr)) == 0) {
        fprintf(stderr, "Invalid address");
        exit(1);
    }
}

static void parse_server_port(const char *arg, struct client_config *config) {
    config->server_addr.sin_port = htons(atoi(arg));

    if (config->server_addr.sin_port < 1 || config->server_addr.sin_port > 65535) {
        fprintf(stderr, "Invalid port");
        exit(1);
    }
}