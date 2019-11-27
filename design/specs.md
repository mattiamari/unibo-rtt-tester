# Assignment 3
Measurement protocol for RTT and throughput in TCP

## Protocol
3 phases:
- Hello (HP)
- Measurement (MP)
- Bye (BP)

### Hello phase

#### Client
1. Wait for TCP connection established.
2. Send message:
    ```<protocol_phase> <sp> <measure_type> <sp> <n_probes> <sp> <msg_size> <sp> <server_delay>\n```

- ```<protocol_phase>``` : value of 'h' for "hello"
- ```<measure_type>``` : "rtt" or "thput", for selecting which measure to make
- ```<n_probes>``` : no. of measurement probes Client will send to Server. Server will echo back each probe.
- ```<msg_size>``` : no. of bytes in probe payload
- ```<server_delay>``` : time for Server to wait before echoing back each probe. Default 0
- ```<sp>``` : field separator. a single whitespace

#### Server
1. Wait for Hello message.
2. Parse Hello message.
3. Reply with "200 OK - Ready"
    or ("404 ERROR - Invalid Hello message" and terminate TCP conn.)


### Measurement phase

#### Client
1. Send ```<n_probes>``` with increasing ```<probe_seq_num>``` starting from 1. Format is
    ```<protocol_phase> <sp> <probe_seq_num> <sp> <payload>\n```
2. Wait for echoed back probes and compute RTT / throughput

- ```<protocol_phase>``` : 'm' as in "measure"
- ```<payload>``` : any text with size ```<msg_size>```

#### Server
1. Wait for probe
2. If valid: echo probe back
3. Keep track of ```<probe_seq_num>``` checking that it's increasing and that does not exceed ```<n_probes>```.
4. If invalid probe (wrong sequence or ```<probe_seq_num>``` > ```<n_probes>```) do not echo back. Send "404 ERROR - Invalid Measurement message" instead and terminate conn.

### Bye phase

#### Client
1. If conn. already closed: NOOP
2. Send message: ```<protocol_phase>```\n
3. Wait for response
4. If response correct: close connection
(not spec) 5. If response invalid: retry until valid

- ```<protocol_phase>``` : 'b' as in "bye"

#### Server
1. Wait for bye message
2. If message correct: send "200 OK - Closing"

## Software behaviour

### Server
- listen port as input param
- print IP and port of client on connect
- print received/sent messages

### Client
- server IP and port as params
- print received/sent messages
- for each probe received, print: seq number, RTT (milliseconds)
- at Measurement end, print: mean RTT (if measurement type = "rtt"), throughput (kbps) (if measurement type = "thput")
