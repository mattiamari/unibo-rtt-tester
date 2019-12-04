#include "protocol.h"

#include <string.h>
#include <stdio.h>
#include <time.h>

const char *response_strings[] = {
    "200 OK - Ready",
    "200 OK - Closing",
    "404 ERROR - Invalid Hello message",
    "404 ERROR - Invalid Measurement message"
};

const unsigned int default_msg_size_rtt[] = {1, 100, 200, 400, 800, 1000};
const unsigned int default_msg_size_thput[] = {1 K, 2 K, 4 K, 16 K, 32 K};

char response_is(char *res, enum response type) {
    return strcmp(res, response_strings[type]) == 0;
}

char is_valid_hello(msg_hello *msg) {
    if (msg->protocol_phase != PHASE_HELLO) {
        return 0;
    }

    if (msg->measure_type == NULL) {
        return 0;
    }

    if (msg->n_probes <= 0) {
        return 0;
    }

    if (msg->msg_size <= 0) {
        return 0;
    }

    if (msg->server_delay < 0) {
        return 0;
    }

    return 1;
}

char is_valid_probe(msg_probe *msg, unsigned int last_seq) {
    if (msg->protocol_phase != PHASE_MEASURE) {
        return 0;
    }

    if (msg->probe_seq_num != last_seq + 1) {
        return 0;
    }

    return 1;
}

char is_valid_bye(msg_bye *msg) {
    if (msg->protocol_phase != PHASE_BYE) {
        return 0;
    }

    return 1;
}

char hello_to_string(msg_hello *msg, char *dest, size_t *size) {
    *size = snprintf(dest, MAX_SIZE_HELLO, "%c %s %u %lu %u\n",
        msg->protocol_phase,
        msg->measure_type,
        msg->n_probes,
        msg->msg_size,
        msg->server_delay);
    
    if (*size >= MAX_SIZE_HELLO) {
        fprintf(stderr, "Output was truncated. Increase DEST size to %lu bytes at least", *size);
        return 0;
    }

    return 1;
}

char probe_to_string(msg_probe *msg, char *dest, size_t *size) {
    *size = snprintf(dest, MAX_SIZE_PROBE, "%c %u %s\n",
        msg->protocol_phase,
        msg->probe_seq_num,
        msg->payload);
    
    if (*size >= MAX_SIZE_PROBE) {
        fprintf(stderr, "Output was truncated. Increase DEST size to %lu bytes at least", *size);
        return 0;
    }

    return 1;
}

char bye_to_string(msg_bye *msg, char *dest, size_t *size) {
    *size = snprintf(dest, MAX_SIZE_BYE, "%c\n", msg->protocol_phase);

    if (*size >= MAX_SIZE_BYE) {
        fprintf(stderr, "Output was truncated. Increase DEST size to %lu bytes at least", *size);
        return 0;
    }

    return 1;
}

char hello_from_string(const char *str, msg_hello *dest) {
    int scan_res;
    char measure_type[8];

    scan_res = sscanf(str, " %c %s %u %lu %u\n",
        &(dest->protocol_phase),
        measure_type,
        &(dest->n_probes),
        &(dest->msg_size),
        &(dest->server_delay));

    if (scan_res < EXPECTED_ITEMS_HELLO) {
        return 0;
    }

    if (strcmp(measure_type, MEASURE_RTT) == 0) {
        dest->measure_type = MEASURE_RTT;
    } else if (strcmp(measure_type, MEASURE_THROUGHPUT) == 0) {
        dest->measure_type = MEASURE_THROUGHPUT;
    } else {
        dest->measure_type = NULL;
    }

    return 1;
}

char probe_from_string(const char *str, msg_probe *dest) {
    int scan_res;

    scan_res = sscanf(str, " %c %u %s\n",
        &(dest->protocol_phase),
        &(dest->probe_seq_num),
        dest->payload);
    
    if (scan_res < EXPECTED_ITEMS_PROBE) {
        return 0;
    }

    return 1;
}

char bye_from_string(const char *str, msg_bye *dest) {
    int scan_res;

    scan_res = sscanf(str, " %c\n", &(dest->protocol_phase));

    if (scan_res < EXPECTED_ITEMS_BYE) {
        return 0;
    }

    return 1;
}

char *new_payload(size_t size) {
    char *payload = calloc(size, sizeof(char));

    srand(time(NULL));

    for (unsigned long i = 0; i < size; i++) {
        payload[i] = 'A' + rand() % ('Z' - 'A');
    }

    return payload;
}