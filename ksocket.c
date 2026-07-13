#include "ksocket.h"

void semwait(int semid, int whichsem){
    struct sembuf sb;

    sb.sem_num = whichsem;
    sb.sem_flg = 0;
    sb.sem_op = -1;
    semop(semid, &sb, 1);
}

void semsignal(int semid, int whichsem){
    struct sembuf sb;

    sb.sem_num = whichsem;
    sb.sem_flg = 0;
    sb.sem_op = 1;
    semop(semid, &sb, 1);
}

void sigint_handler(int sig){
    int shm_socket, shm_sockinfo, sem_join, sem_createsock;
    key_t key_socket, key_sockinfo, key_join, key_createsock;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    shmctl(shm_socket, IPC_RMID, NULL);

    key_sockinfo = ftok(KEYFILE, KEY_SOCKINFO);
    shm_sockinfo = shmget(key_sockinfo, sizeof(ktp_sockinfo), 0777 | IPC_CREAT);
    shmctl(shm_sockinfo, IPC_RMID, NULL);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);
    semctl(sem_join, 0, IPC_RMID, NULL);

    key_createsock = ftok(KEYFILE, KEY_CREATE_SOCKET);
    sem_createsock = semget(key_createsock, 3, 0777 | IPC_CREAT);
    semctl(sem_createsock, 0, IPC_RMID, NULL);
    
    exit(0);
}

int k_socket(int domain, int type, int protocol){
    // printf("K_SOCKET: Inside k_socket\n");
    signal(SIGINT, sigint_handler);

    if(type != SOCK_KTP){
        errno = EINVAL;
        return -1;
    }

    if(domain != AF_INET){
        errno = EINVAL;
        return -1;
    }

    int shm_socket, shm_sockinfo, sem_join, sem_createsock;
    key_t key_socket, key_sockinfo, key_join, key_createsock;

    ktp_socket *SHM;
    ktp_sockinfo *SHMINF;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);

    key_sockinfo = ftok(KEYFILE, KEY_SOCKINFO);
    shm_sockinfo = shmget(key_sockinfo, sizeof(ktp_sockinfo), 0777 | IPC_CREAT);
    SHMINF = (ktp_sockinfo *)shmat(shm_sockinfo, NULL, 0);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    key_createsock = ftok(KEYFILE, KEY_CREATE_SOCKET);
    sem_createsock = semget(key_createsock, 3, 0777 | IPC_CREAT);

    semwait(sem_createsock, 2);

    SHMINF -> sockfd = 0;
    SHMINF -> error = 0;

    semsignal(sem_createsock, 0);
    semwait(sem_createsock, 1);

    errno = 0;

    if(SHMINF -> error != 0){
        errno = SHMINF -> error;
        SHMINF -> error = 0;
        SHMINF -> sockfd = 0;
        semsignal(sem_createsock, 2);
        return -1;
    }

    for(int i = 0; i < MAX_SOCKETS; i++){
        semwait(sem_join, i);

        if(SHM[i].allocated == 0){
            SHM[i].allocated = 1;
            SHM[i].pid = getpid();

            SHM[i].sockfd = SHMINF -> sockfd;
            #ifdef DEBUG
                // printf("K_SOCKET: sockfd: %d\n", SHM[i].sockfd);
            #endif

            semsignal(sem_join, i);

            SHMINF -> sockfd = 0;
            semsignal(sem_createsock, 2);
            return i;
        }

        semsignal(sem_join, i);
    }

    errno = ENOSPACE;
    SHMINF -> sockfd = 0;
    semsignal(sem_createsock, 2);
    return -1;
}
    

int k_bind(int sockfd, char *src_ip, int src_port, char *dest_ip, int dest_port){

    signal(SIGINT, sigint_handler);

    int shm_socket, shm_sockinfo, sem_join, sem_createsock;
    key_t key_socket, key_sockinfo, key_join, key_createsock;

    ktp_socket *SHM;
    ktp_sockinfo *SHMINF;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);

    key_sockinfo = ftok(KEYFILE, KEY_SOCKINFO);
    shm_sockinfo = shmget(key_sockinfo, sizeof(ktp_sockinfo),  0777 | IPC_CREAT);
    SHMINF = (ktp_sockinfo *)shmat(shm_sockinfo, NULL, 0);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    key_createsock = ftok(KEYFILE, KEY_CREATE_SOCKET);
    sem_createsock = semget(key_createsock, 3, 0777 | IPC_CREAT);


    semwait(sem_createsock, 2);



    SHMINF -> sockfd = SHM[sockfd].sockfd;
    strcpy(SHMINF -> src_ip, src_ip);
    SHMINF -> src_port = src_port;
    SHMINF -> error = 0;
    
    semsignal(sem_createsock, 0);
    

    semwait(sem_createsock, 1);


    if(SHMINF -> error != 0){
        errno = SHMINF -> error;

        SHMINF -> sockfd = 0;
        SHMINF -> error = 0;
        memset(SHMINF -> src_ip, 0, sizeof(SHMINF -> src_ip));
        SHMINF -> src_port = 0;
        semsignal(sem_createsock, 2);

        return -1;
    }


    semwait(sem_join, sockfd);


    if(SHM[sockfd].allocated == 1){
        strcpy(SHM[sockfd].src_ip, src_ip);
        SHM[sockfd].src_port = src_port;
        strcpy(SHM[sockfd].dest_ip, dest_ip);
        SHM[sockfd].dest_port = dest_port;

        semsignal(sem_join, sockfd);


        SHMINF -> sockfd = 0;
        SHMINF -> error = 0;
        memset(SHMINF -> src_ip, 0, sizeof(SHMINF -> src_ip));
        SHMINF -> src_port = 0;

        semsignal(sem_createsock, 2);

        return 0;
    }

    SHMINF -> sockfd = 0;
    SHMINF -> error = 0;
    memset(SHMINF -> src_ip, 0, sizeof(SHMINF -> src_ip));
    SHMINF -> src_port = 0;

    errno = ENOSPACE;

    semsignal(sem_createsock, 2);

    return -1;
}

ssize_t k_sendto(int sockfd, const void *buf, size_t len, int flags,
                      const struct sockaddr *dest_addr, socklen_t addrlen){
    signal(SIGINT, sigint_handler);

    int shm_socket, sem_join;
    key_t key_socket, key_join;

    ktp_socket *SHM;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    if(len > MAX_MSG){
        printf("Maximum message size is %d bytes\n", MAX_MSG);
        return -1;
    }

    if(len < 0){
        printf("Invalid message length\n");
        return -1;
    }

    if(SHM[sockfd].dest_port != ntohs(((struct sockaddr_in*)dest_addr)->sin_port) ||
       strcmp(SHM[sockfd].dest_ip, inet_ntoa(((struct sockaddr_in*)dest_addr)->sin_addr)) != 0){
        #ifdef DEBUG
            printf("SENDTO: Destination port or IP address does not match\n");
        #endif
        errno = ENOTBOUND;
        return -1;
    }
    
    semwait(sem_join, sockfd);
    errno = 0;

    if(SHM[sockfd].allocated == 1){
        int it = SHM[sockfd].swnd_ptr[0];
        int flag = 0;
        while(it < SWND && SHM[sockfd].swnd.seqno[it] != -1 && flag < SWND){
            flag++;
            it = (it + 1) % SWND;
        }

        if(SHM[sockfd].swnd.seqno[it] == -1){
            SHM[sockfd].sendlen[it] = len;
            memcpy(SHM[sockfd].sendbuffer[it], buf, SHM[sockfd].sendlen[it]);
            SHM[sockfd].swnd.seqno[it] = SHM[sockfd].nextseqno;
            SHM[sockfd].nextseqno = (SHM[sockfd].nextseqno + 1) % MAX_SEQNO;

            if(SHM[sockfd].nextseqno == 0){
                SHM[sockfd].nextseqno = 1;
            }

            semsignal(sem_join, sockfd);
            return len;
        }
    }

    semsignal(sem_join, sockfd);
    errno = ENOSPACE;
    return -1;
}

ssize_t k_recvfrom(int sockfd, void *buf, size_t len, int flags,
                        struct sockaddr *src_addr, socklen_t *addrlen){


    signal(SIGINT, sigint_handler);

    int shm_socket, sem_join;
    key_t key_socket, key_join;

    ktp_socket *SHM;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);


    semwait(sem_join, sockfd);

    errno = 0;

    for(int i = 0; i < RWND; i++){
        if(SHM[sockfd].recv_status[i] == SHM[sockfd].rwnd_ptr[2]){

            memcpy(buf, SHM[sockfd].recvbuffer[i], SHM[sockfd].recvlen[i]);
            int checklen = SHM[sockfd].recvlen[i];

            if(len < checklen){
                semsignal(sem_join, sockfd);
                printf("Length of buffer (%ld) is lesser than the message length (%d)\n", len, checklen);
                return -1;
            }

            SHM[sockfd].recv_status[i] = 0;
            SHM[sockfd].recvlen[i] = 0;

            SHM[sockfd].rwnd_ptr[2] = (SHM[sockfd].rwnd_ptr[2] + 1) % MAX_SEQNO;

            if(SHM[sockfd].rwnd_ptr[2] == 0){
                SHM[sockfd].rwnd_ptr[2] = 1;
            }

            if(SHM[sockfd].rwnd.size < MAX_WND){
                SHM[sockfd].rwnd.size++;
                SHM[sockfd].rwnd_ptr[1] = (SHM[sockfd].rwnd_ptr[1] + 1) % RWND;
            }

            semsignal(sem_join, sockfd);

            return checklen;
        }
    }

    semsignal(sem_join, sockfd);
    return -1;
}

int k_close(int sockfd){
    signal(SIGINT, sigint_handler);

    int shm_socket, shm_sockinfo, sem_join, sem_createsock;
    key_t key_socket, key_sockinfo, key_join, key_createsock;

    ktp_socket *SHM;
    ktp_sockinfo *SHMINF;
    
    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);

    key_sockinfo = ftok(KEYFILE, KEY_SOCKINFO);
    shm_sockinfo = shmget(key_sockinfo, sizeof(ktp_sockinfo), 0777 | IPC_CREAT);
    SHMINF = (ktp_sockinfo *)shmat(shm_sockinfo, NULL, 0);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    key_createsock = ftok(KEYFILE, KEY_CREATE_SOCKET);
    sem_createsock = semget(key_createsock, 3, 0777 | IPC_CREAT);
    
    semwait(sem_createsock, 2);

    SHMINF -> sockfd = SHM[sockfd].sockfd;
    SHMINF -> error = 0;

    SHMINF -> src_port = -1;

    semsignal(sem_createsock, 0);
    semwait(sem_createsock, 1);

    if(SHMINF -> error != 0){
        errno = SHMINF -> error;

        SHMINF -> sockfd = 0;
        SHMINF -> error = 0;
        SHMINF -> src_port = 0;

        semsignal(sem_createsock, 2);
        return -1;
    }

    semwait(sem_join, sockfd);
    
    if(SHM[sockfd].allocated == 1){
        printf("SEND COUNT: %d\n", SHM[sockfd].send_count);
        SHM[sockfd].allocated = 0;
        SHM[sockfd].nospace = 0;
        
        memset(SHM[sockfd].src_ip, 0, sizeof(SHM[sockfd].src_ip));
        memset(SHM[sockfd].dest_ip, 0, sizeof(SHM[sockfd].dest_ip));

        memset(SHM[sockfd].sendbuffer, 0, sizeof(SHM[sockfd].sendbuffer));
        memset(SHM[sockfd].recvbuffer, 0, sizeof(SHM[sockfd].recvbuffer));
        
        memset(SHM[sockfd].sendlen, 0, sizeof(SHM[sockfd].sendlen));
        memset(SHM[sockfd].recvlen, 0, sizeof(SHM[sockfd].recvlen));

        SHM[sockfd].nextseqno = 1;
        SHM[sockfd].expectseqno = 1;
        
        for(int j = 0; j < MAX_WND; j++){
            SHM[sockfd].swnd.seqno[j] = -1;
            SHM[sockfd].rwnd.seqno[j] = -1;

            SHM[sockfd].send_timer[j].tv_sec = 0;
            SHM[sockfd].send_timer[j].tv_usec = 0;

            SHM[sockfd].recv_status[j] = 0;
        }

        SHM[sockfd].swnd.size = MAX_WND;
        SHM[sockfd].rwnd.size = MAX_WND;

        SHM[sockfd].swnd_ptr[0] = 0;
        SHM[sockfd].swnd_ptr[1] = MAX_WND - 1;

        SHM[sockfd].rwnd_ptr[0] = 0;
        SHM[sockfd].rwnd_ptr[1] = MAX_WND - 1;
        SHM[sockfd].rwnd_ptr[2] = 1;

        semsignal(sem_join, sockfd);

        SHMINF -> sockfd = 0;
        SHMINF -> error = 0;
        SHMINF -> src_port = 0;

        SHM[sockfd].send_count = 0;
        printf("CLOSE: Socket %d allocated\n", sockfd);
        semsignal(sem_createsock, 2);
        return 0;
    }

    semsignal(sem_join, sockfd);
    SHMINF -> sockfd = 0;
    SHMINF -> error = 0;
    SHMINF -> src_port = 0;

    semsignal(sem_createsock, 2);
    return -1;
}

int dropMessage(float _p){
    float r = (float)rand() / RAND_MAX;
    return r < _p;
}
