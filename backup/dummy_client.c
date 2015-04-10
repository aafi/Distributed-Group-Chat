#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h> 
#include <string.h>
#define BUFLEN 512
#define PORT 9930

void err(char *s)
{
    perror(s);
    exit(1);
}

void send_msg(int sockfd, char msg[BUFLEN], struct sockaddr_in serv_addr, socklen_t slen)
{
    if (sendto(sockfd, msg, BUFLEN, 0, (struct sockaddr*) &serv_addr, slen)==-1)
            err("sendto()");   
}

void receive_msg(int sockfd, char msg[BUFLEN], struct sockaddr_in *serv_addr, socklen_t *slen)
{
    if (recvfrom(sockfd, msg, BUFLEN, 0, (struct sockaddr*)&serv_addr, slen)==-1)
            err("recvfrom()");
}
 
int main(int argc, char** argv)
{
    struct sockaddr_in serv_addr, my_addr;
    int sockfd, i;
    socklen_t slen=sizeof(serv_addr);
    char buf[BUFLEN], status[20];
    if(argc != 2)
    {
      printf("Usage : %s <client_port>\n",argv[0]);
      exit(0);
    }
    
    //strcpy(buf, argv[3]);
    
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0))==-1)
        err("socket");

    bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(atoi(argv[1]));
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
 
    // bzero(&serv_addr, sizeof(serv_addr));
    // serv_addr.sin_family = AF_INET;
    // serv_addr.sin_port = htons(atoi(argv[2]));
    // if (inet_aton(argv[1], &serv_addr.sin_addr)==0)
    // {
    //     fprintf(stderr, "inet_aton() failed\n");
    //     exit(1);
    // }

    if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1)
      err("bind");

    //printf("Into while\n");
 
    while(1)
    {
        receive_msg(sockfd, buf, &serv_addr, &slen); //MSG = "ELECTION"
        //send_msg(sockfd, "active", serv_addr, slen);       
        //sprintf(buf, "%s", argv[2]);
        //printf("buf %s\n", buf);        
        printf("%s\n", buf);
        receive_msg(sockfd, buf, &serv_addr, &slen);
        printf("%s\n", buf);

        //exit(0);
    }
 
    close(sockfd);
    return 0;
}
