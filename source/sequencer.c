
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/queue.h>

#define PORT 5678
#define NPACK 10
#define BUFLEN 1024
#define MAX 15

int id[MAX] = {0};
int msg_seq_id = 1;

struct client{
   char ip[BUFLEN];
   int port;
   int client_id;
   int last_msg_id;     //id of the last message sent by the client
  // int leader;          //by default is 0. The client which is the leader will have 1
}client_list[MAX];

struct message{
   int client_id;
   int msg_id;
   char msg[BUFLEN];
}


int requestid(char * ip, int port)
{
   int i;
   for(i=0;i<MAX;i++)
   {
      if(id[i]==0)
      {  
         struct client c;
         strcpy(c.ip,ip);
         c.port = port;
         c.last_msg_id = 0;
         c.client_id = i;
        // c.leader = 0;
         client_list[i] = c;
         id[i] = 1;
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
         int i=0;
         while(token !=NULL)
         {
            token = strtok(NULL,"#");
            tok[i] = token;
            i++;
         }

         seq = requestid(tok[0],atoi(tok[1]));   // Gets back a sequence number for the new client

         if(seq == -1)
            strcpy(reply,"Max Limit reached");
         else
            sprintf(reply, "%d", seq);

         if((sendto(s,reply,sizeof(reply),0,(struct sockaddr*)&client, sizeof(client))) < 0)    //send reply back
         {
            perror("Send Error");
            exit(-1);
         }

         char multi[BUFLEN] = "Seq";
         char temp[BUFLEN];

         int i = 0;
         for(i;i<MAX;i++)
         {
            if(id[i]!=0)
            {
               strcat(multi,"#");
               strcat(multi,client_list[i].ip);
               strcat(multi,"#");
               sprintf(temp,"%d",client_list[i].port);
               strcat(multi,temp);
               strcat(multi,"#");
               sprintf(temp,"%d",client_list[i].client_id);
               strcat(multi,temp);

            }

         }

      }

      else if (strcmp("Message",token)==0)
      {
         while(token!=NULL)
         {  
            int i = 0;
            token = strtok(NULL,"#");
            tok[i] = token;
            i++;
         }

         int client_id,msg_id;
         char msg[BUFLEN];

         client_id = atoi(tok[0]);
         msg_id = atoi(tok[1]);
         strcpy(msg,tok[2]);

         
      }


   //MULTICAST PART ----- DO IT
      int k = 0;
      for (k;k<MAX;k++)
      {

      }

   }



}





