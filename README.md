# MTP: Modified Transport Protocol

## Where UDP Gets Its Act Together
Turning packet chaos into orderly communication, one acknowledgment at a time.

## Overview

MTP (Modified Transport Protocol) is a custom implementation of a reliable transport protocol built on top of UDP. This project demonstrates the principles of network protocols, concurrency, and error handling in networked systems.

## Documentation

Complete Documentation is present at [doc](./documentation.txt) 

## Features

- Reliable data transfer over unreliable UDP
- Sliding window flow control
- Multi-threaded architecture for concurrent operations
- Shared memory IPC supporting up to 25 simultaneous MTP sockets
- Simulated unreliable network conditions for robustness testing

## Project Structure

- `msocket.h` and `msocket.c`: Core MTP implementation
- `initmsocket.c`: Initializes MTP sockets, starts threads and garbage collector
- `user1.c` and `user2.c`: Example applications using MTP sockets
- `Makefile`: For compiling the project

## Installation

1. Clone the repository:
   ```
   git clone https://github.com/lurkingryuu/mtp_socket.git
   cd mtp-protocol
   ```

2. Compile the project:
   ```
   make
   ```

## Usage

1. Start the MTP initialization process:
   ```
   make runinit
   ```

2. In separate terminals, run the sender and receiver:
   ```
   ./sender -p 8080 -h 127.0.0.1 -P 9090 -H 127.0.0.1 -f sample_100kB.txt
   ./receiver -p 9090 -h 127.0.0.1 -P 8080 -H 127.0.0.1 -f received.txt
   ```

## Multi-user Test

Run multiple sender-receiver pairs:

```
./sender -p 8080 -h 127.0.0.1 -P 9090 -H 127.0.0.1 -f sample_100kB.txt
./receiver -p 9090 -h 127.0.0.1 -P 8080 -H 127.0.0.1 -f received.txt
./sender -p 8081 -h 127.0.0.1 -P 9091 -H 127.0.0.1 -f sample_100kB.txt
./receiver -p 9091 -h 127.0.0.1 -P 8081 -H 127.0.0.1 -f received2.txt
```

## Performance Analysis

| Probability | Messages Sent | Messages Received | Messages Dropped |
|-------------|---------------|-------------------|------------------|
| 0.05        | 213           | 200               | 13               |
| 0.10        | 246           | 200               | 42               |
| ...         | ...           | ...               | ...              |
| 0.50        | 756           | 200               | 561              |

## Cleaning Up

To remove compiled files:

```
make clean
```

## Authors

- Yelisetty Karthikeya S M (21CS30060)
- Vonteri Varshith Reddy (21CS10081)

