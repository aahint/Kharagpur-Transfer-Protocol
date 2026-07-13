#ifndef KSOCKET_H
#define KSOCKET_H

#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sem.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
/* Socket type */
#define SOCK_KTP 1
/* Maximum number of sockets */
#define MAX_SOCKETS 10
/* Timeout */
#define T 5
/* Drop probability */
#define P 0.05
/* Sender Window Size */
#define SWND 10
/* Receiver Window Size */
#define RWND 10
/* Message size */
#define MAX_MSG 512
/* Max sequence number */
#define MAX_SEQNO 256
/* Max window size */
#define MAX_WND 10

/* KEYFILE */
#define KEYFILE "ksocket.h"

/* Keys for Shared Memory and Semaphores */
#define KEY_JOIN 'J'
#define KEY_CREATE_SOCKET 'C'
#define KEY_SOCKET 'S'
#define KEY_SOCKINFO 'I'

/* Error codes */
#define ENOSPACE    124 
#define ENOTBOUND   125 
#define ENOMESSAGE  126 


typedef struct {
    int size;
    int seqno[MAX_WND];
} wnd;


typedef struct {
    int sockfd;
    pid_t pid;

    /* Flags */
    int allocated;
    int nospace;

    /* Socket addresses */
    char src_ip[16];
    char dest_ip[16];
    int src_port;
    int dest_port;

    /* Buffers */
    char sendbuffer[SWND][MAX_MSG];
    char recvbuffer[RWND][MAX_MSG];

    /* Buffer lengths */
    int sendlen[SWND];
    int recvlen[RWND];

    /* Window sizes */
    wnd swnd;
    wnd rwnd;

    /* Sequence numbers */
    int nextseqno;
    int expectseqno;

    /* Timers */
    struct timeval send_timer[SWND];

    /* Receiver status */
    int recv_status[RWND];

    /* Pointers */
    int swnd_ptr[2];
    int rwnd_ptr[3];

    /* Number of messages sent */
    int send_count;
} ktp_socket;

typedef struct {
    int sockfd;
    char src_ip[16];
    int src_port;
    int error;
} ktp_sockinfo;

/* Semaphore Operations */
void semwait(int semid, int whichsem);
void semsignal(int semid, int whichsem);

/* Signal handler for SIGINT */
void sigint_handler(int sig);

int k_socket(int domain, int type, int protocol);
int k_bind(int sockfd, char* src_ip, int src_port, char* dest_ip, int dest_port);
ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen);
ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen);
int k_close(int sockfd);

int dropMessage(float _p);

#endif