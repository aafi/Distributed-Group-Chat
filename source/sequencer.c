
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>




#define PORT 5678
#define NPACK 10
#define BUFLEN 1024
#define MAX 15

int id[MAX] = {0};
int msg_seq_id = 0;

struct client{
   char ip[BUFLEN];
   int port;
   int client_id;
   int last_msg_id;     //id of the last message sent by the client
  // int leader;          //by default is 0. The client which is the leader will have 1
}client_list[MAX];

struct message{
   int seq_id;
   int client_id;
   int msg_id;
   char msg[BUFLEN];

   /*
         * This holds the pointers to the next and previous entries in
         * the tail queue.
    */

   TAILQ_ENTRY(message) entries;  
};

//This is the head of the TAILQ

TAILQ_HEAD(,message) message_head;


/*
  
   Function to multicast a message to all clients available
      
*/


void multicast(int socket,char * msg)
{
   int idx = 0;
   for(idx;idx<MAX;idx++)
   {
      if(id[idx]!=0)
      {
         struct sockaddr_in clnt;
         clnt.sin_family = AF_INET;
         clnt.sin_port = client_list[idx].port;
         clnt.sin_addr.s_addr = inet_addr(client_list[idx].ip);

         printf("%s %d %s \n",msg,client_list[idx].port,client_list[idx].ip);
         if((sendto(socket,msg,BUFLEN,0,(struct sockaddr *)&clnt, sizeof(clnt))) < 0)
         {
            perror("Broadcast Error");
            exit(-1);
         }
         
         // printf("DONE \n");

      }
   }
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

void detokenize(char buf[], char* token_result[], char* token){
   char* result;
   int i = 0;
   result = strtok(buf, token);
   token_result[i++] = result;
   while(result != NULL){
      result = strtok(NULL, token);
      if (result != NULL){
         token_result[i++] = result;
      }
   }
}


const char* get_ip_address(){
   FILE *fp;
   int status;
   char shell_output[BUFLEN];

   fp = popen("/sbin/ifconfig | grep inet | head -n 1", "r");
   if (fp == NULL)
       perror("Could not get IP address");

   fgets(shell_output, BUFLEN, fp);

    status = pclose(fp);
   if (status == -1) {
       perror("Error closing fp");
   }

   char* shell_result[BUFLEN];
    detokenize(shell_output, shell_result, " ");

   char* addr_info[BUFLEN];
   detokenize(shell_result[1], addr_info, ":");

   return addr_info[1];
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


   /*
  
         BROADCAST IP and PORT to all clients after winning election and declare leader
         SEQ#EA#IP#PORT

   */

   char win_broadcast[BUFLEN]="SEQ#EA#";
   char tmp[BUFLEN];
   const char* my_ip_addr = get_ip_address();
   strcat(win_broadcast,my_ip_addr);
   strcat(win_broadcast,"#");
   sprintf(tmp,"%d",PORT);
   strcat(win_broadcast,tmp);

   multicast(s,win_broadcast);


   /* Initialize the tail queue */
   TAILQ_INIT(&message_head);

   while(1)
   {


      if((n = recvfrom(s, buf, BUFLEN, 0,(struct sockaddr*)&client, &len)) < 0)
      {
         perror("Receive Error");
         exit(-1);
      }

      char * token;
      token = strtok(buf,"#");

      if (strcmp("REQUEST",token)==0)
      {
         int seq;
         int i = 0;
         while(token !=NULL)
         {
            token = strtok(NULL,"#");
            tok[i] = token;
            i++;
         }

         seq = requestid(tok[0],atoi(tok[1]));   // Gets back a sequence number for the new client

         if(seq == -1)
         {
            strcpy(reply,"FAILURE");
         }
         else
         {
            char tmp[BUFLEN];
            sprintf(tmp, "%d", seq);
            strcpy(reply,"SUCCESS#");
            strcat(reply,tmp);
         }

         if((sendto(s,reply,sizeof(reply),0,(struct sockaddr*)&client, sizeof(client))) < 0)    //send reply back
         {
            perror("Send Error");
            exit(-1);
         }

         char multi[BUFLEN] = "SEQ#CLIENTINFO";
         char temp[BUFLEN];

         int d = 0;
         for(d;d<MAX;d++)
         {
            if(id[d]!=0)
            {
               strcat(multi,"#");
               strcat(multi,client_list[d].ip);
               strcat(multi,"#");
               sprintf(temp,"%d",client_list[d].port);
               strcat(multi,temp);
               strcat(multi,"#");
               sprintf(temp,"%d",client_list[d].client_id);
               strcat(multi,temp);

            }

         }
         // printf("%s \n",multi);
         multicast(s,multi);

      }

      else if (strcmp("MESSAGE",token)==0)
      {
         while(token!=NULL)
         {  
            int i = 0;
            token = strtok(NULL,"#");
            tok[i] = token;
            i++;
         }

         struct message *item;
         item = malloc(sizeof(*item));
         item->client_id = atoi(tok[0]);
         item->msg_id = atoi(tok[1]);
         strcpy(item->msg,tok[2]);
         item->seq_id = msg_seq_id++;

         int idx = 0;
         for(idx;idx<MAX;idx++)
         {
            if(id[idx]!=0)
            {
               if(client_list[idx].client_id == atoi(tok[0]))
                  client_list[idx].last_msg_id = atoi(tok[1]);
            }
         }

         /*
                 * Add our item to the end of tail queue. The first
                 * argument is a pointer to the head of our tail
                 * queue, the second is the item we want to add, and
                 * the third argument is the name of the struct
                 * variable that points to the next and previous items
                 * in the tail queue.
         */

         TAILQ_INSERT_TAIL(&message_head,item,entries);




      }


   /* Traverse the tail queue forward. */
        printf("Forward traversal: ");

        struct message *item;

        TAILQ_FOREACH(item, &message_head, entries) {
                printf("%d %s \n",item->seq_id,item->msg);
        }


   //MULTICAST PART ----- DO IT
      // int k = 0;
      // for (k;k<MAX;k++)
      // {

      // }

   }



}