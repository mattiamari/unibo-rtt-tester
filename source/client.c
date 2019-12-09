#define _GNU_SOURCE

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

static char doc[] = "RTT and throughput tester. Client software.";
static char args_doc[] = "SERVER_ADDR PORT";

static struct argp_option options[] = {
    {"measure", 'm', "TYPE", 0, "Type of measure to perform (rtt | thput). Defaults to 'rtt'.", 1},
    {"n-probes", 'n', "NUM", 0, "Number of probes to send, Defaults to 20.", 1},
    {"size", 's', "BYTES", 0, "Size of the probe's payload.", 1},
    {"server-delay", 'd', "MS", 0, "Server artificial delay in milliseconds. Defaults to 0.", 1},
    {0}
};



static void state_hello();
static void state_measure();
static void state_bye();
static void state_wait_bye_resp();
static void state_close();

static void handle_terminate(int sig);

static error_t arg_parser(int key, char *arg, struct argp_state *state);
static void parse_measure_type(const char *arg, struct client_config *config);
static void parse_probe_num(const char *arg, struct client_config *config);
static void parse_payload_size(const char *arg, struct client_config *config);
static void parse_server_addr(const char *arg, struct client_config *config);
static void parse_server_port(const char *arg, struct client_config *config);
static void parse_server_delay(const char *arg, struct client_config *config);



static struct argp argp = {options, arg_parser, args_doc, doc, 0, 0, 0};
static struct client_config config;
static unsigned short current_state;
static int sock;
static char recv_buf[RECV_BUF_SIZE];
static msg_hello hello_message;
static int curr_payload_size_idx;

int main(int argc, char **argv) {
    config.n_probes = 20;
    config.server_delay = 0;
    config.measure_type = MEASURE_RTT;
    config.payload_sizes = default_payload_size_rtt;
    config.n_sizes = sizeof default_payload_size_rtt / sizeof default_payload_size_rtt[0];

    bzero(&(config.server_addr), sizeof(struct sockaddr_in));
    config.server_addr.sin_family = AF_INET;
    curr_payload_size_idx = 0;

    if (argp_parse(&argp, argc, argv, 0, 0, &config) != 0) {
        fprintf(stderr, "Some error occurred while parsing arguments\n");
        exit(1);
    }

    signal(SIGINT, handle_terminate);

    current_state = STATE_HELLO;

    while(1) {
        switch (current_state) {
            case STATE_HELLO        : state_hello(); break;
            case STATE_MEASURE      : state_measure(); break;
            case STATE_BYE          : state_bye(); break;
            case STATE_WAIT_BYE_RESP: state_wait_bye_resp(); break;
            case STATE_CLOSE        : state_close();
        }
    }

    return 0;
}

static void state_hello() {
    struct timeval timeout;
    char addr_str[INET_ADDRSTRLEN];
    size_t msg_str_len;
    char msg_str[MAX_SIZE_HELLO];

    hello_message.protocol_phase = PHASE_HELLO;
    hello_message.measure_type = config.measure_type;
    hello_message.n_probes = config.n_probes;
    hello_message.msg_size = config.payload_sizes[curr_payload_size_idx];
    hello_message.server_delay = config.server_delay;

    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // Timeout recv operations after SOCK_TIMEOUT_SEC seconds
    timeout.tv_usec = 0;
    timeout.tv_sec = SOCK_TIMEOUT_SEC;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout)) == -1) {
        perror("Cannot set socket options");
        current_state = STATE_CLOSE;
        return;
    }
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout)) == -1) {
        perror("Cannot set socket options");
        current_state = STATE_CLOSE;
        return;
    }

    if (sock == -1) {
        perror("Cannot create socket");
        exit(errno);
    }

    if (connect(sock, &(config.server_addr), sizeof(config.server_addr)) == -1) {
        perror("Cannot connect host");
        exit(errno);
    }

    inet_ntop(AF_INET, &(config.server_addr.sin_addr), addr_str, INET_ADDRSTRLEN);
    printf("Connected to %s on port %d\n", addr_str, config.server_addr.sin_port);

    if (!hello_to_string(&hello_message, msg_str, &msg_str_len)) {
        fprintf(stderr, "Cannot serialize Hello message");
        current_state = STATE_CLOSE;
        return;
    }

    printf("Sending hello message. (%lu bytes)\n", msg_str_len);
    print_send(msg_str);
    send(sock, msg_str, msg_str_len, 0);

    if (recv(sock, recv_buf, RECV_BUF_SIZE, 0) == -1) {
        perror("Error occurred while waiting for Hello response");
        current_state = STATE_CLOSE;
        return;
    }

    print_recv(recv_buf);

    if (!response_is(recv_buf, RESP_READY)) {
        fprintf(stderr, "Invalid response");
        current_state = STATE_CLOSE;
        return;
    }

    current_state = STATE_MEASURE;
}

static void state_measure() {
    char *payload;
    msg_probe probe, echoed_probe;
    char probe_str[MAX_SIZE_PROBE];
    size_t probe_str_len;
    char probe_buf[RECV_BUF_SIZE];
    ssize_t echoed_probe_size = 0;
    size_t recv_idx = 0;
    struct timeval time_before, time_after;
    unsigned long rtt_sum = 0, rtt_min = ULONG_MAX, rtt_max = 0;
    unsigned long curr_rtt;
    double avg_rtt_sec, probe_kbits;

    printf("Starting measure. measure_type=%s n_probes=%d msg_size=%lu server_delay=%d\n",
        measure_types_strings[hello_message.measure_type], hello_message.n_probes,
        hello_message.msg_size, hello_message.server_delay);

    payload = new_payload(hello_message.msg_size);
    probe.protocol_phase = PHASE_MEASURE;
    probe.payload = payload;

    for (unsigned int i = 1; i <= hello_message.n_probes; i++) {
        probe.probe_seq_num = i;

        if (!probe_to_string(&probe, probe_str, &probe_str_len)) {
            fprintf(stderr, "Cannot serialize probe");
            free(payload);
            current_state = STATE_CLOSE;
            return;
        }

        #ifdef DEBUG
        print_send(probe_str);
        #endif

        send(sock, probe_str, probe_str_len, 0);
        gettimeofday(&time_before, NULL);
        printf("Sent probe seq %d / %d (%lu bytes) ... ", probe.probe_seq_num, hello_message.n_probes, probe_str_len);
        fflush(stdout);

        // Wait and check echoed probe
        echoed_probe_size = recv_until(sock, recv_buf, RECV_BUF_SIZE, &recv_idx, probe_buf, RECV_BUF_SIZE, '\n');
        gettimeofday(&time_after, NULL);

        if (echoed_probe_size == -1) {
            perror("Receive error");
            free(payload);
            current_state = STATE_CLOSE;
            return;
        }

        if (echoed_probe_size == -2) {
            fprintf(stderr, "Probe buffer too small\n");
            free(payload);
            current_state = STATE_CLOSE;
            return;
        }

        #ifdef DEBUG
        print_recv(probe_buf);
        #endif

        if (!probe_from_string(probe_buf, &echoed_probe)
            || !is_valid_probe(&echoed_probe, probe.probe_seq_num)
        ) {
            fprintf(stderr, "Received invalid echoed probe\n");
            free(payload);
            current_state = STATE_CLOSE;
            return;
        }

        curr_rtt = get_diff_ms(&time_before, &time_after);
        rtt_sum += curr_rtt;
        rtt_min = ulmin(rtt_min, curr_rtt);
        rtt_max = ulmax(rtt_max, curr_rtt);
        printf("RTT = %ld ms\n", curr_rtt);
    }

    printf("\nRTT min / max / avg = %ld / %ld / %ld ms\n\n", rtt_min, rtt_max, rtt_sum / hello_message.n_probes);

    if (hello_message.measure_type == MEASURE_THPUT) {
        avg_rtt_sec = rtt_sum / hello_message.n_probes / 1000.0;
        probe_kbits = echoed_probe_size * 8 / 1000.0;
        printf("THROUGHPUT = %ld kbits/sec\n", (unsigned long)(probe_kbits / avg_rtt_sec));
    }

    free(payload);
    current_state = STATE_BYE;
}

static void state_bye() {
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

static void state_wait_bye_resp() {
    printf("Waiting bye response\n");

    if (recv(sock, recv_buf, RECV_BUF_SIZE, 0) == -1) {
        if (errno == ETIMEDOUT) {
            printf(".");
        } else {
            perror("Receive error");
            current_state = STATE_CLOSE;
            return;
        }
    }

    print_recv(recv_buf);

    if (!response_is(recv_buf, RESP_CLOSING)) {
        fprintf(stderr, "Invalid Bye response\n");
        current_state = STATE_CLOSE;
        return;
    }

    if (curr_payload_size_idx < config.n_sizes - 1) {
        curr_payload_size_idx += 1;
        current_state = STATE_HELLO;
        close(sock);
        return;
    }

    current_state = STATE_CLOSE;
}

static void state_close() {
    printf("Closing\n");
    close(sock);
    exit(EXIT_SUCCESS);
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
static void handle_terminate(int sig) {
    printf("Interrupt caught. Exiting.\n");
    close(sock);
    exit(EXIT_SUCCESS);
}
#pragma GCC diagnostic pop

static error_t arg_parser(int key, char *arg, struct argp_state *state) {
    struct client_config *config = state->input;

    switch (key) {
        case 'm': parse_measure_type(arg, config); break;
        case 'n': parse_probe_num(arg, config); break;
        case 's': parse_payload_size(arg, config); break;
        case 'd': parse_server_delay(arg, config); break;

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
        config->payload_sizes = default_payload_size_rtt;
        config->n_sizes = sizeof default_payload_size_rtt / sizeof default_payload_size_rtt[0];
    } else if (strcmp("thput", arg) == 0) {
        config->measure_type = MEASURE_THPUT;
        config->payload_sizes = default_payload_size_thput;
        config->n_sizes = sizeof default_payload_size_thput / sizeof default_payload_size_thput[0];
    } else {
        fprintf(stderr, "Invalid measure type\n");
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
    int port = atoi(arg);

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port");
        exit(1);
    }

    config->server_addr.sin_port = htons(port);
}

static void parse_server_delay(const char *arg, struct client_config *config) {
    int delay = atoi(arg);

    if (delay < 0) {
        fprintf(stderr, "Invalid server delay");
        exit(1);
    }

    config->server_delay = delay;
}
