#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdlib.h>

#define EXPECTED_ITEMS_HELLO 5
#define EXPECTED_ITEMS_PROBE 3
#define EXPECTED_ITEMS_BYE 1

#define K * 1024
#define MAX_SIZE_HELLO 32
#define MAX_SIZE_BYE 4

// probe size needs to accomodate a max of 32K payload, plus the overhead
#define MAX_SIZE_PROBE 33 K

#define MEASURE_RTT "rtt"
#define MEASURE_THROUGHPUT "thput"

#define PHASE_HELLO 'h'
#define PHASE_MEASURE 'm'
#define PHASE_BYE 'b'

// i.e. h thput 200 32768 100
typedef struct msg_hello_s {
    char protocol_phase;
    char *measure_type;
    unsigned int n_probes;
    size_t msg_size;
    unsigned int server_delay;
} msg_hello;

typedef struct msg_probe_s {
    char protocol_phase;
    unsigned int probe_seq_num;
    char *payload;
} msg_probe;

typedef struct msg_bye_s {
    char protocol_phase;
} msg_bye;

// These indexes must match the ones in response_strings[]
enum response {
    RESP_READY = 0,
    RESP_CLOSING,
    RESP_INVALID_HELLO,
    RESP_INVALID_PROBE
};

char response_is(char *res, enum response type);

char is_valid_hello(msg_hello *msg);
char is_valid_probe(msg_probe *msg, unsigned int last_seq);
char is_valid_bye(msg_bye *msg);

char hello_to_string(msg_hello *msg, char *dest, size_t *size);
char probe_to_string(msg_probe *msg, char *dest, size_t *size);
char bye_to_string(msg_bye *msg, char *dest, size_t *size);

char hello_from_string(const char *str, msg_hello *dest);
char probe_from_string(const char *str, msg_probe *dest);
char bye_from_string(const char *str, msg_bye *dest);

char *new_payload(size_t size);

#endif