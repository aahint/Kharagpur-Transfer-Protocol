#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ksocket.h"

int main(int argc, char *argv[])
{   
    if (argc != 6) {
        printf("Usage: %s <local_ip> <local_port> <remote_ip> <remote_port> <input_filename>\n", argv[0]);
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

    struct sockaddr_in dest_addr;
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(remote_port);
    dest_addr.sin_addr.s_addr = inet_addr(remote_ip);

    if(k_bind(sockfd, local_ip, local_port, remote_ip, remote_port) < 0)
    {
        perror("bind");
        exit(1);
    }

    printf("Bind successful\n");

    int send_count = 0, nbytes;

    char buffer[512];
    bzero(buffer, sizeof(buffer));

    char *filename = argv[5];

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("File open failed");
        exit(1);
    }
    
    while(nbytes = read(fileno(fp), buffer, sizeof(buffer)))
    {
        int send_len = k_sendto(sockfd, buffer, nbytes, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        send_count++;

        while(send_len < 0)
        {
            sleep(1);
            send_len = k_sendto(sockfd, buffer, nbytes, 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
            send_count++;
        }

        if(buffer[nbytes - 1] == '\n'){
            break;
        }

        bzero(buffer, sizeof(buffer));
    }
    sleep(100);

    fclose(fp);
    k_close(sockfd);    
    printf("Total k_sendto() calls sent: %d\n", send_count);
    return 0;
}