#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ksocket.h"

int main(int argc, char *argv[])
{   
    if (argc != 6) {
        printf("Usage: %s <local_ip> <local_port> <remote_ip> <remote_port> <output_filename>\n", argv[0]);
        return 1;
    }
    
    char *local_ip = argv[1];
    int local_port = atoi(argv[2]);
    char *remote_ip = argv[3];
    int remote_port = atoi(argv[4]);

    int sockfd;

    if((sockfd = k_socket(AF_INET, SOCK_KTP, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }
    printf("Socket created\n");
    if(k_bind(sockfd, local_ip, local_port, remote_ip, remote_port) < 0)
    {
        perror("bind");
        exit(1);
    }
    printf("Bind successful\n");
    struct sockaddr_in src_addr;
    src_addr.sin_family = AF_INET;
    src_addr.sin_port = htons(remote_port);
    src_addr.sin_addr.s_addr = inet_addr(remote_ip);

    int len = sizeof(src_addr);

    char *filename = argv[5];

    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("File open failed");
        exit(1);
    }

    char buffer[512];
    bzero(buffer, sizeof(buffer));

    int recv_count = 0, nbytes;

    while(1){
        nbytes = k_recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr *)&src_addr, &len);
        recv_count++;

        if(nbytes > 0){
            write(fileno(fp), buffer, nbytes);
            if(buffer[nbytes - 1] == '\n'){
                break;
            }
        }
        else{
            sleep(1);
        }
        bzero(buffer, sizeof(buffer));
    }
    sleep(100);
    fclose(fp);
    k_close(sockfd);
    printf("Total k_recvfrom() calls made: %d\n", recv_count);
    return 0;
}