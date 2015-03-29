
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>

#define PORT 5678
#define NPACK 10
#define BUFLEN 512
#define MAX 15

char * id[MAX] = {NULL};

int requestid(char * name)
{
   int i;
   for(i=0;i<MAX;i++)
   {
      if(id[i]==NULL)
      {  
         strcpy(id[i],name);
         return i;
      }
   }
   return -1;                    //MAX limit reached; New participant cannot be added

}

int main(int argc, char *argv[]){
   struct sockaddr_in server, client;
   int s,n, len = sizeof(client);
   char buf[BUFLEN],reply[BUFLEN];
   char * tok[BUFLEN];

   const char * temp;

   if((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("Socket error");
      exit(-1);
   }

   server.sin_family = AF_INET;
   server.sin_port = htons(PORT);
   server.sin_addr.s_addr = htonl(INADDR_ANY);

   if(bind(s, (struct sockaddr*)&server, sizeof(server)) < 0){
      perror("Bind error");
      exit(-1);
   }

   while(1)
   {
      if((n = recvfrom(s, buf, BUFLEN, 0,(struct sockaddr*)&client, &len)) < 0)
      {
         perror("Receive Error");
         exit(-1);
      }

      char * token;
      token = strtok(buf,"#");

      if (strcmp("Request",token)==0)
      {
         int seq;
         token = strtok(NULL, "#"); // Tokenizes received message to get name of the new client
         seq = requestid(token);   // Gets back a sequence number for the new client
         if(seq == -1)
            strcpy(reply,"Max Limit reached");
         else
            sprintf(reply, "%d", seq);
      }

      else if (strcmp("Message",token)==0)
      {
         

      }


      if((sendto(s,reply,sizeof(reply),0,(struct sockaddr*)&client, sizeof(client))) < 0)    //send reply back
      {
         perror("Send Error");
         exit(-1);
      }

   }



}





// int main(int argc, char *argv[])
// {
//    int sockfd,n;
//    struct sockaddr_in servaddr,cliaddr;
//    socklen_t len;
//    char mesg[1000];

//    sockfd=socket(AF_INET,SOCK_DGRAM,0);

//    bzero(&servaddr,sizeof(servaddr));
//    servaddr.sin_family = AF_INET;
//    servaddr.sin_addr.s_addr=htonl(INADDR_ANY);
//    servaddr.sin_port=htons(32000);
//    bind(sockfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

//    while(1)
//    {
//       len = sizeof(cliaddr);
//       n = recvfrom(sockfd,mesg,1000,0,(struct sockaddr *)&cliaddr,&len);
//       sendto(sockfd,mesg,n,0,(struct sockaddr *)&cliaddr,sizeof(cliaddr));
//       printf("-------------------------------------------------------\n");
//       mesg[n] = 0;
//       printf("Received the following:\n");
//       printf("%s",mesg);
//       printf("-------------------------------------------------------\n");
//    }
// }