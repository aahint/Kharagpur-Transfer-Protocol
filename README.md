# KTP: Kharagpur Transfer Protocol

A reliable, TCP-style message transport written in C, built on top of an unreliable datagram link. It uses sliding windows, sequence numbers, ACKs and timeout-based retransmission, and it exposes a socket-like API (`k_socket`, `k_bind`, `k_sendto`, ...).

## Why this project
UDP-style links can drop or reorder packets. KTP adds the reliability layer an application would otherwise have to write itself, and it lets you test that layer by *deliberately dropping packets*.

## How it works
- **Sliding window:** sender and receiver each use a window of 10 messages.
- **Sequence numbers:** 8-bit sequence space (0 to 255), 512-byte messages.
- **Retransmission:** unacknowledged messages are resent after a 5-second timeout.
- **Loss simulation:** each packet is dropped with probability 0.05, so you can watch recovery happen.
- **Shared state:** socket tables live in shared memory, guarded by System V semaphores. A background process (`initksocket.c`) does the actual sending and receiving, while user programs call the `k_*` API.

## Files
| File | Purpose |
|---|---|
| `ksocket.h` / `ksocket.c` | KTP API, data structures, constants |
| `initksocket.c` | Background process that owns the real sockets and runs the protocol |
| `user1.c` | Sender: sends `send.txt` |
| `user2.c` | Receiver: writes the data to `recv.txt` |
| `makefile` | Build rules |

## Build and run
```bash
make
./initksocket          # terminal 1: start the protocol process
./user2                # terminal 2: start the receiver
./user1                # terminal 3: start the sender
```

## What I learned
Flow control with windows, retransmission timers, shared memory and semaphore synchronisation, and testing network code under packet loss.
