#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdlib.h>

/**
 * Expected items to be parsed by scanf when reading a serialized Hello.
 */
#define EXPECTED_ITEMS_HELLO 5

/**
 * Expected items to be parsed by scanf when reading a serialized Probe.
 */
#define EXPECTED_ITEMS_PROBE 3

/**
 * Expected items to be parsed by scanf when reading a serialized Bye.
 */
#define EXPECTED_ITEMS_BYE 1

/**
 * I'm lazy
 */
#define K * 1024

/**
 * Maximum size a serialized Hello can be.
 */
#define MAX_SIZE_HELLO 32

/**
 * Maximum size a serialized Bye can be.
 */
#define MAX_SIZE_BYE 4

/**
 * Maximum size a serialized Probe can be.
 * Probe size needs to accomodate a max of 32K payload, plus the overhead.
 */
#define MAX_SIZE_PROBE 33 K

/**
 * Char for the Hello phase
 */
#define PHASE_HELLO 'h'

/**
 * Char for the Measure phase
 */
#define PHASE_MEASURE 'm'

/**
 * Char for the Bye phase
 */
#define PHASE_BYE 'b'

/**
 * Types of measures that can be performed.
 * The indexes match the ones in measure_types_strings[]
 */
enum measure_types {
    MEASURE_RTT = 1,
    MEASURE_THPUT
};

/**
 * Response messages.
 * The indexes match the ones in response_strings[]
 */
enum responses {
    RESP_READY = 1,
    RESP_CLOSING,
    RESP_INVALID_HELLO,
    RESP_INVALID_PROBE
};

/**
 * Hello message
 */
typedef struct msg_hello_s {
    char protocol_phase;
    enum measure_types measure_type;
    unsigned int n_probes;
    size_t msg_size;
    unsigned int server_delay;
} msg_hello;

/**
 * Probe message
 */
typedef struct msg_probe_s {
    char protocol_phase;
    unsigned int probe_seq_num;
    char *payload;
} msg_probe;

/**
 * Bye message
 */
typedef struct msg_bye_s {
    char protocol_phase;
} msg_bye;

/**
 * Check if response (as string) matches the specified response from [enum responses]
 */
char response_is(char *res, enum responses type);

/**
 * Check if the provided Hello message is valid
 */
char is_valid_hello(msg_hello *msg);

/**
 * Check if the provided Probe message is valid.
 * Sequence number is checked against LAST_SEQ
 */
char is_valid_probe(msg_probe *msg, unsigned int last_seq);

/**
 * Check if the provided Bye message is valid
 */
char is_valid_bye(msg_bye *msg);

/**
 * Serialize an Hello struct to its corresponding string representation to be sent via socket.
 * String actual length is written in SIZE
 */
int hello_to_string(msg_hello *msg, char *dest, size_t *size);

/**
 * Serialize an Probe struct to its corresponding string representation to be sent via socket.
 * String actual length is written in SIZE
 */
int probe_to_string(msg_probe *msg, char *dest, size_t *size);

/**
 * Serialize an Bye struct to its corresponding string representation to be sent via socket.
 * String actual length is written in SIZE
 */
int bye_to_string(msg_bye *msg, char *dest, size_t *size);

/**
 * Deserialize input string to its corresponding Hello struct
 */
int hello_from_string(const char *str, msg_hello *dest);

/**
 * Deserialize input string to its corresponding Probe struct
 */
int probe_from_string(const char *str, msg_probe *dest);

/**
 * Deserialize input string to its corresponding Bye struct
 */
int bye_from_string(const char *str, msg_bye *dest);

/**
 * Allocate a new random payload of SIZE bytes
 */
char *new_payload(size_t size);

#endif