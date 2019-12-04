#include "client.h"

#include "utils.h"
#include "protocol.h"

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <strings.h>
#include <string.h>

static unsigned short current_state;
static int sock;
static char recv_buf[RECV_BUF_SIZE];
static msg_hello hello_message;

int main(int argc, char **argv) {
    int port;
    struct sockaddr_in server_addr;
    struct timeval timeout;

    if (argc < 3) {
        print_usage();
        return 1;
    }

    port = strtol(argv[2], NULL, 10);

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port");
        return 1;
    }

    bzero(&server_addr, sizeof(server_addr));

    if (inet_aton(argv[1], &(server_addr.sin_addr)) == 0) {
        fprintf(stderr, "Invalid address");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

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

    if (connect(sock, &server_addr, sizeof(server_addr)) == -1) {
        perror("Cannot connect host");
        return errno;
    }

    printf("Connected to %s on port %d\n", argv[1], port);

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
    hello_message.measure_type = MEASURE_RTT;
    hello_message.n_probes = 20;
    hello_message.msg_size = 10;
    hello_message.server_delay = 0;

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

void print_usage() {
    printf("Usage: ./client SERVER_ADDR PORT\n");
}

void handle_terminate(int sig) {
    printf("Caught SIGINT. Closing.\n");
    close(sock);
}