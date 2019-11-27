# Pseudocode

## Client

### Hello
```c++

connection = connect(ip_addr, port)
sendHello()
response = waitReply()

if response == "200 OK - Ready"
    enterState(MEASUREMENT)
else
    close(connection)
    exit()

```

### Measurement
```c++

for seq = 1 to n_probes {
    sendProbe(seq, msg_size)
    waitEcho(seq)
    computeRTT(seq)
}

computeAvgRTT()

if measurementMode == THPUT
    computeAvgThroughput()

enterState(BYE)

```

### Bye
```c++

if isClosed(connection)
    exit()

sendBye()
response = waitReply()

if response == OK {
    close(connection)
    exit()
}

```

## Server

### Hello
```c++

socketListen()
connection = accept()
message = waitMessage()

if !isValidHello(message) {
    send("404 ERROR - Invalid Hello message")
    close(connection)
    exit()
}

send("200 OK - Ready")
enterState(MEASUREMENT)

```

### Measurement
```c++

recv_probe_no = 0

while probe = waitProbe() {
    recv_probe_no += 1

    if !isValidProbe(probe) || recv_probe_no > n_probes
        send("404 ERROR - Invalid Measurement message")
        close(connection)
        exit()
    
    send(probe)
}

enterState(BYE)
    
```

## Bye
```c++

message = waitBye()

if isValidBye(message)
    send("200 OK - Closing")

```