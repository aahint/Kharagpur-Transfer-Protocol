#include "ksocket.h"

int sendack(int sockindex, int sockfd, int prevack, int rwndsize){
    signal(SIGINT, sigint_handler);

    int shm_socket, sem_join;
    key_t key_socket, key_join;

    ktp_socket *SHM;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    unsigned char ackheader[4];

    ackheader[0] = 0;
    ackheader[1] = (unsigned char)(prevack);
    ackheader[2] = (unsigned char)(rwndsize);
    ackheader[3] = '\0';

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SHM[sockindex].dest_port);
    addr.sin_addr.s_addr = inet_addr(SHM[sockindex].dest_ip);

    int nbytes = sendto(sockfd, ackheader, 4, 0, (struct sockaddr *)&addr, sizeof(addr));

    #ifdef DEBUG
        printf("%d: Sent ACK with seqno %d and rwnd %d\n", sockindex, ackheader[1], ackheader[2]);
    #endif

    return nbytes;
}

void *S(void *arg){
    signal(SIGINT, sigint_handler);

    int shm_socket, sem_join;
    key_t key_socket, key_join;

    ktp_socket *SHM;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);
    
    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    while(1){
        for(int i = 0; i < MAX_SOCKETS; i++){
            semwait(sem_join, i);
            struct timeval current_time;
            gettimeofday(&current_time, NULL);

            if(SHM[i].allocated == 0){
                semsignal(sem_join, i);
                continue;
            }

            long long duration = ((current_time.tv_sec - SHM[i].send_timer[SHM[i].swnd_ptr[0]].tv_sec) * 1000000
                + (current_time.tv_usec - SHM[i].send_timer[SHM[i].swnd_ptr[0]].tv_usec))/ 1000000;
            
            if(duration > T &&
                (SHM[i].send_timer[SHM[i].swnd_ptr[0]].tv_sec != 0 || 
                    SHM[i].send_timer[SHM[i].swnd_ptr[0]].tv_usec != 0)){

                int it = SHM[i].swnd_ptr[0], jt = 0;
                while(jt < SHM[i].swnd.size && it < SWND){
                    if(SHM[i].swnd.seqno[it] != -1 && SHM[i].allocated == 1){

                        unsigned char buffer[MAX_MSG + 1];
                        memset(buffer, 0, sizeof(buffer));
                        buffer[0] = (unsigned char)(SHM[i].swnd.seqno[it]);

                        memcpy(buffer + 1, SHM[i].sendbuffer[it], SHM[i].sendlen[it]);
                    
                        struct sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(SHM[i].dest_port);
                        addr.sin_addr.s_addr = inet_addr(SHM[i].dest_ip);

                        int len = sizeof(addr);

                        int nbytes = sendto(SHM[i].sockfd, buffer, SHM[i].sendlen[it] + 1, 0,
                            (const struct sockaddr *)&addr, len);

                        if(nbytes == -1) continue;

                        SHM[i].send_count++;

                        #ifdef DEBUG
                            printf("%d: Sent message with seqno %d\n", i, SHM[i].swnd.seqno[it]);
                        #endif

                        gettimeofday(&current_time, NULL);
                        SHM[i].send_timer[it] = current_time;
                    }
                    it = (it + 1) % SWND;
                    jt++;
                }   
            }
            else{
                int it = SHM[i].swnd_ptr[0], jt = 0;
                while(jt < SHM[i].swnd.size && it < SWND){
                    if (SHM[i].send_timer[it].tv_sec == 0 && SHM[i].send_timer[it].tv_usec == 0 && SHM[i].allocated == 1 && SHM[i].swnd.seqno[it] != -1){
                        unsigned char buffer[MAX_MSG + 1];
                        memset(buffer, 0, sizeof(buffer));
                        buffer[0] = (unsigned char)(SHM[i].swnd.seqno[it]);
                        memcpy(buffer + 1, SHM[i].sendbuffer[it], SHM[i].sendlen[it]);

                        struct sockaddr_in addr;
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(SHM[i].dest_port);
                        addr.sin_addr.s_addr = inet_addr(SHM[i].dest_ip);

                        int len = sizeof(addr);

                        int nbytes = sendto(SHM[i].sockfd, buffer, SHM[i].sendlen[it] + 1, 0,
                            (const struct sockaddr *)&addr, len);

                        if(nbytes == -1){
                            it = (it + 1) % SWND;
                            continue;
                        }

                        SHM[i].send_count++;

                        #ifdef DEBUG
                            printf("%d: Sent message with seqno %d\n", i, SHM[i].swnd.seqno[it]);
                        #endif

                        gettimeofday(&current_time, NULL);
                        SHM[i].send_timer[it] = current_time;
                    }
                    it = (it + 1) % SWND;
                    jt++;
                }   
            }
            semsignal(sem_join, i);
        }
        sleep(T/2);
    }
    pthread_exit(NULL);
}

void *R(void *arg){
    signal(SIGINT, sigint_handler);

    int shm_socket, sem_join;
    key_t key_socket, key_join;

    ktp_socket *SHM;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    while(1){
        fd_set readfds;
        FD_ZERO(&readfds);

        int maxfd = 0;

        for(int i = 0; i < MAX_SOCKETS; i++){
            semwait(sem_join, i);

            if(SHM[i].allocated != 0){
                FD_SET(SHM[i].sockfd, &readfds);
                if(SHM[i].sockfd > maxfd) maxfd = SHM[i].sockfd;
            }
            semsignal(sem_join, i);
        }

        struct timeval timeout;
        timeout.tv_sec = T;
        timeout.tv_usec = 0;

        int nready = select(maxfd + 1, &readfds, NULL, NULL, &timeout);
        if(nready == -1){
            perror("select");
            exit(1);
        }

        for(int i = 0; i < MAX_SOCKETS; i++){
            semwait(sem_join, i);

            if(SHM[i].allocated != 0){
                
                // 1. CONCURRENCY FIX: Independent Window Update
                // This evaluates immediately on EVERY loop!
                if(SHM[i].nospace == 1 && SHM[i].rwnd.size > 0){
                    int prevack = SHM[i].expectseqno - 1;
                    if(prevack == 0) prevack = MAX_SEQNO - 1;
                    int acklen = sendack(i, SHM[i].sockfd, prevack, SHM[i].rwnd.size);

                    // Turn off flag immediately
                    SHM[i].nospace = 0; 
                }

                // 2. Incoming Data Processing
                if(nready > 0 && FD_ISSET(SHM[i].sockfd, &readfds)){
                    struct sockaddr_in src_addr;
                    socklen_t addrlen = sizeof(src_addr);

                    unsigned char buffer[MAX_MSG + 1];
                    memset(buffer, 0, sizeof(buffer));

                    int nbytes = recvfrom(SHM[i].sockfd, buffer, MAX_MSG + 1, 0, (struct sockaddr *)&src_addr, &addrlen);

                    if(nbytes == -1){
                        semsignal(sem_join, i);
                        continue;
                    }

                    if(dropMessage(P) == 1){
                        #ifdef DEBUG
                            printf("%d: Dropped message with seqno %d\n", i, buffer[0]);
                        #endif
                        semsignal(sem_join, i);
                        continue;
                    }

                    uint8_t seqno = (uint8_t)buffer[0];

                    if(seqno > 0 && seqno < MAX_SEQNO){
                        int expseq = SHM[i].expectseqno;
                        int it = SHM[i].rwnd_ptr[0], jt = 0, flag = 0;
                        while(jt < SHM[i].rwnd.size && it < RWND){
                            if(seqno == expseq){
                                SHM[i].nospace = 0; 

                                if(it == SHM[i].rwnd_ptr[0]){
                                    memcpy(SHM[i].recvbuffer[it], buffer + 1, nbytes - 1);
                                    SHM[i].recvlen[it] = nbytes - 1;
                                    SHM[i].recv_status[it] = seqno;

                                    while(jt < SHM[i].rwnd.size && it < RWND && SHM[i].recv_status[it] != 0){
                                        SHM[i].rwnd_ptr[0] = (SHM[i].rwnd_ptr[0] + 1) % RWND;
                                        if(SHM[i].recv_status[(SHM[i].rwnd_ptr[1] + 1) % RWND] == 0){
                                            SHM[i].rwnd_ptr[1] = (SHM[i].rwnd_ptr[1] + 1) % RWND;
                                        }
                                        else{
                                            SHM[i].rwnd.size--;
                                        }
                                        SHM[i].expectseqno = (SHM[i].expectseqno + 1) % MAX_SEQNO;
                                        it = (it + 1) % RWND;
                                        jt++;
                                    }

                                    if(SHM[i].rwnd.size == 0) SHM[i].nospace = 1;
                                    int prevack = SHM[i].expectseqno - 1;
                                    if(prevack == 0) prevack = MAX_SEQNO - 1;
                                    int acklen = sendack(i, SHM[i].sockfd, prevack, SHM[i].rwnd.size);

                                    if (SHM[i].rwnd.size == 0) {
                                        SHM[i].nospace = 1;
                                    }
                                    flag = 1;
                                    break;
                                }
                                else{
                                    memcpy(SHM[i].recvbuffer[it], buffer + 1, nbytes - 1);
                                    SHM[i].recvlen[it] = nbytes - 1;
                                    SHM[i].recv_status[it] = seqno;
                                    flag = 1;
                                    break;
                                }
                            }
                            it = (it + 1) % RWND;
                            jt++;
                            expseq = (expseq + 1) % MAX_SEQNO;
                            if(expseq == 0) expseq = 1;
                        }
                        if(flag == 0){
                            int prevack = SHM[i].expectseqno - 1;
                            if(prevack == 0) prevack = MAX_SEQNO - 1;
                            int acklen = sendack(i, SHM[i].sockfd, prevack, SHM[i].rwnd.size);

                            if (SHM[i].rwnd.size == 0) {
                                SHM[i].nospace = 1;
                            }
                        }
                    }
                    else if(seqno == 0){
                        uint8_t wndsize, prevack;
                        prevack = (uint8_t)(buffer[1]);
                        wndsize = (uint8_t)(buffer[2]);

                        int it = SHM[i].swnd_ptr[0], jt = 0;
                        if(SHM[i].swnd.size == 0){
                            if(wndsize > 0){
                                SHM[i].swnd_ptr[1] = (SHM[i].swnd_ptr[0] + wndsize - 1) % SWND;
                                SHM[i].swnd.size = wndsize;
                            }
                            semsignal(sem_join, i);
                            continue;
                        }
                        while(jt < SHM[i].swnd.size){
                            if(SHM[i].swnd.seqno[it] == prevack) break;
                            it = (it + 1) % SWND;
                            jt++;
                        }
                        if(jt == SHM[i].swnd.size){
                            semsignal(sem_join, i);
                            continue;
                        }
                        int kt = (it + 1) % SWND;
                        it = SHM[i].swnd_ptr[0];
                        int lt = 0;

                        while(lt <= jt && it < SWND){
                            memset(SHM[i].sendbuffer[it], 0, MAX_MSG);
                            SHM[i].sendlen[it] = 0;
                            SHM[i].send_timer[it].tv_sec = 0;
                            SHM[i].send_timer[it].tv_usec = 0;
                            SHM[i].swnd.seqno[it] = -1;
                            it = (it + 1) % SWND;
                            lt++;
                        }

                        SHM[i].swnd_ptr[0] = kt;
                        if(wndsize != 0){
                            SHM[i].swnd_ptr[1] = (SHM[i].swnd_ptr[0] + wndsize - 1) % SWND;
                        }
                        SHM[i].swnd.size = wndsize;
                    }
                }
            }
            semsignal(sem_join, i);
        }
    }   
    pthread_exit(NULL);
}

void *G(void *arg){
    signal(SIGINT, sigint_handler);

    int shm_socket, sem_join;
    key_t key_socket, key_join;

    ktp_socket *SHM;

    key_socket = ftok(KEYFILE, KEY_SOCKET);
    shm_socket = shmget(key_socket, sizeof(ktp_socket) * MAX_SOCKETS, 0777 | IPC_CREAT);
    SHM = (ktp_socket *)shmat(shm_socket, NULL, 0);

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    while(1){
        for(int i = 0; i < MAX_SOCKETS; i++){
            semwait(sem_join, i);

            if(SHM[i].allocated == 0){
                semsignal(sem_join, i);
                continue;
            }

            int status;
            int check = waitpid(SHM[i].pid, &status, WNOHANG);
            if(check == 0){
                semsignal(sem_join, i);
                continue;
            }

            struct stat st;
            char path[1024];
            memset(path, 0, sizeof(path));
            sprintf(path, "/proc/%d", SHM[i].pid);
            if((stat(path, &st) == -1 && errno == ENOENT)){
                close(SHM[i].sockfd);
                SHM[i].allocated = 0;
                SHM[i].nospace = 0;
                
                memset(SHM[i].src_ip, 0, sizeof(SHM[i].src_ip));
                memset(SHM[i].dest_ip, 0, sizeof(SHM[i].dest_ip));

                memset(SHM[i].sendbuffer, 0, sizeof(SHM[i].sendbuffer));
                memset(SHM[i].recvbuffer, 0, sizeof(SHM[i].recvbuffer));
                
                memset(SHM[i].sendlen, 0, sizeof(SHM[i].sendlen));
                memset(SHM[i].recvlen, 0, sizeof(SHM[i].recvlen));

                SHM[i].nextseqno = 1;
                SHM[i].expectseqno = 1;
                
                for(int j = 0; j < MAX_WND; j++){
                    SHM[i].swnd.seqno[j] = -1;
                    SHM[i].rwnd.seqno[j] = -1;

                    SHM[i].send_timer[j].tv_sec = 0;
                    SHM[i].send_timer[j].tv_usec = 0;

                    SHM[i].recv_status[j] = 0;
                }

                SHM[i].swnd.size = MAX_WND;
                SHM[i].rwnd.size = MAX_WND;

                SHM[i].swnd_ptr[0] = 0;
                SHM[i].swnd_ptr[1] = MAX_WND - 1;

                SHM[i].rwnd_ptr[0] = 0;
                SHM[i].rwnd_ptr[1] = MAX_WND - 1;
                SHM[i].rwnd_ptr[2] = 1; 

                SHM[i].send_count = 0;
            }
            semsignal(sem_join, i);
        }
        sleep(T*2);
    }
    pthread_exit(NULL);
}

int main(){
    srand(time(NULL));

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

    SHMINF -> sockfd = 0;
    memset(SHMINF -> src_ip, 0, sizeof(SHMINF -> src_ip));
    SHMINF -> src_port = 0;
    SHMINF -> error = 0;

    key_join = ftok(KEYFILE, KEY_JOIN);
    sem_join = semget(key_join, MAX_SOCKETS, 0777 | IPC_CREAT);

    key_createsock = ftok(KEYFILE, KEY_CREATE_SOCKET);
    sem_createsock = semget(key_createsock, 3, 0777 | IPC_CREAT);

    semctl(sem_createsock, 0, SETVAL, 0);
    semctl(sem_createsock, 1, SETVAL, 0);
    semctl(sem_createsock, 2, SETVAL, 1);

    for(int i = 0; i < MAX_SOCKETS; i++){
        semctl(sem_join, i, SETVAL, 1);
    }

    for(int i = 0; i < MAX_SOCKETS; i++){
        SHM[i].allocated = 0;
        SHM[i].nospace = 0;

        memset(SHM[i].src_ip, 0, sizeof(SHM[i].src_ip));
        memset(SHM[i].dest_ip, 0, sizeof(SHM[i].dest_ip));

        memset(SHM[i].sendbuffer, 0, sizeof(SHM[i].sendbuffer));
        memset(SHM[i].recvbuffer, 0, sizeof(SHM[i].recvbuffer));

        memset(SHM[i].sendlen, 0, sizeof(SHM[i].sendlen));
        memset(SHM[i].recvlen, 0, sizeof(SHM[i].recvlen));

        SHM[i].nextseqno = 1;
        SHM[i].expectseqno = 1;

        for(int j = 0; j < MAX_WND; j++){
            SHM[i].swnd.seqno[j] = -1;
            SHM[i].rwnd.seqno[j] = -1;

            SHM[i].send_timer[j].tv_sec = 0;
            SHM[i].send_timer[j].tv_usec = 0;

            SHM[i].recv_status[j] = 0;
        }

        SHM[i].swnd.size = MAX_WND;
        SHM[i].rwnd.size = MAX_WND;

        SHM[i].swnd_ptr[0] = 0;
        SHM[i].swnd_ptr[1] = MAX_WND - 1;

        SHM[i].rwnd_ptr[0] = 0;
        SHM[i].rwnd_ptr[1] = MAX_WND - 1;
        SHM[i].rwnd_ptr[2] = 1;

        SHM[i].send_count = 0;
    }

    pthread_t Sid, Rid, Gid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    pthread_create(&Rid, &attr, R, NULL);
    pthread_create(&Sid, &attr, S, NULL);
    pthread_create(&Gid, &attr, G, NULL);

    while(1){
        semwait(sem_createsock, 0);
        
        if(SHMINF -> sockfd == 0 && SHMINF -> src_port == 0){
            int sockfd;
            if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1){
                SHMINF -> error = errno;
                semsignal(sem_createsock, 0);
                continue;
            }

            SHMINF -> sockfd = sockfd;
            semsignal(sem_createsock, 1);
        }
        else if(SHMINF -> sockfd != 0 && SHMINF -> src_port >= 0){
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_port = htons(SHMINF -> src_port);
            addr.sin_addr.s_addr = inet_addr(SHMINF -> src_ip);

            if(bind(SHMINF -> sockfd, (struct sockaddr *)&addr, sizeof(addr)) == -1){
                SHMINF -> error = errno;
                semsignal(sem_createsock, 1);
                continue;
            }

            SHMINF -> error = 0;
            semsignal(sem_createsock, 1);
            continue;
        }
        else if(SHMINF -> src_port == -1){
            if((close(SHMINF -> sockfd)) == -1){
                SHMINF -> error = errno;
                semsignal(sem_createsock, 1);
                continue;
            }
            semsignal(sem_createsock, 1);
        }
    }

    pthread_join(Sid, NULL);
    pthread_join(Rid, NULL);
    pthread_join(Gid, NULL);

    shmctl(shm_socket, IPC_RMID, 0);
    shmctl(shm_sockinfo, IPC_RMID, 0);
    semctl(sem_createsock, 0, IPC_RMID);
    semctl(sem_join, 0, IPC_RMID);
    return 0;
}