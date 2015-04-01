
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>




#define PORT 5678
#define PORT_PING 5679
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
         clnt.sin_port = htons(client_list[idx].port);
         clnt.sin_addr.s_addr = inet_addr(client_list[idx].ip);

         //printf("%s %d %s \n",msg,client_list[idx].port,client_list[idx].ip);
         if((sendto(socket,msg,BUFLEN,0,(struct sockaddr *)&clnt, sizeof(clnt))) < 0)
         {
            perror("Broadcast Error");
            exit(-1);
         }
         
         // printf("DONE \n");

      }
   }
}

int count_clients()
{
  int num_client = 0;
  int idx = 0;
  for(idx;idx<MAX;idx++)
  {
    if(id[idx]==1)
      num_client++;
  }

  return num_client;

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


void* message_receiving(int s)
{
  char * tok[BUFLEN];
  struct sockaddr_in client;
  int n, len = sizeof(client);
  char buf[BUFLEN],reply[BUFLEN];
  const char * temp;
  int socket = s; 

  while(1)
   {


      if((n = recvfrom(socket, buf, BUFLEN, 0,(struct sockaddr*)&client, &len)) < 0)
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

         if((sendto(socket,reply,sizeof(reply),0,(struct sockaddr*)&client, sizeof(client))) < 0)    //send reply back
         {
            perror("Send Error");
            exit(-1);
         }

         char multi[BUFLEN] = "SEQ#CLIENTINFO#";
         char temp[BUFLEN];
         int num_client = count_clients();
         sprintf(temp,"%d",num_client);
         strcat(multi,temp);


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
         
         multicast(socket,multi);

         
      }

      else if (strcmp("MESSAGE",token)==0)
      {
         int i = 0;
         while(token!=NULL)
         {  
            
            token = strtok(NULL,"#");
            tok[i] = token;
            i++;
         }

         struct message *item;
        //printf("before malloc \n");
         item = malloc(sizeof(*item));
       // printf("after malloc \n");
         item->client_id = atoi(tok[0]);
        // printf("%d\n", item->client_id);
         item->msg_id = atoi(tok[1]);
         strcpy(item->msg,tok[2]);
         item->seq_id = msg_seq_id++;

         /*
            UPDATING LAST MESSAGE ID AS THE LAST MESSAGE ID RECEIVED FROM A CLIENT
         */

         // int idx = 0;
         // for(idx;idx<MAX;idx++)
         // {
         //    if(id[idx]!=0)
         //    {
         //       if(client_list[idx].client_id == atoi(tok[0]))
         //          client_list[idx].last_msg_id = atoi(tok[1]);
         //    }
         // }

         /*
                 * Add our item to the end of tail queue. The first
                 * argument is a pointer to the head of our tail
                 * queue, the second is the item we want to add, and
                 * the third argument is the name of the struct
                 * variable that points to the next and previous items
                 * in the tail queue.
         */

         TAILQ_INSERT_TAIL(&message_head,item,entries);

         /*
            Send acknowledgement back to the client that message has been received and put in the queue
          
         */

        char ack[BUFLEN] = "SEQ#MSG#ACK";

        if((sendto(socket,ack,BUFLEN,0,(struct sockaddr *)&client, sizeof(client))) < 0)
       {
          perror("Acknowledgement Error");
          exit(-1);
       }
   
     }

   }

}




void* message_multicasting(int s)
{
  int socket = s;
  while(1)
  {
    if(!TAILQ_EMPTY(&message_head))
    {
      struct message *item;
      TAILQ_FOREACH(item, &message_head, entries)
      {
          int idx = 0, flag = 0;
          for(idx;idx<MAX;idx++)
          {
            if(id[idx]!=0)
            {
              /*
              Finding the right client structure
              */

              if(item->client_id == client_list[idx].client_id)
              {
                  char msg[BUFLEN] = "MSG#";
                  char temp[BUFLEN];
                  sprintf(temp,"%d",item->seq_id);
                  strcat(msg,temp);
                  strcat(msg,"#");
                  sprintf(temp,"%d",item->client_id);
                  strcat(msg,temp);
                  strcat(msg,"#");
                  sprintf(temp,"%d",item->msg_id);
                  strcat(msg,temp);
                  strcat(msg,"#");
                  strcat(msg,item->msg);

              /*
              CHECK IF THE MESSAGE AT THE TOP IS THE ONE TO BE SENT NEXT
              */

                int next_msg = client_list[idx].last_msg_id+1;
                if(item->msg_id == next_msg)
                {
                  
                  multicast(socket,msg);
                  client_list[idx].last_msg_id = item->msg_id;
                  TAILQ_REMOVE(&message_head,item,entries);
                  free(item);
                  flag = 1;
                }

                /*
                TRAVERSE THROUGH THE LIST TO FIND IF THE NEXT MESSAGE TO BE SENT EXISTS
                */

                else
                { 
                  struct message *next;
                  TAILQ_FOREACH(next, &message_head, entries)
                  {
                    if(next->msg_id == next_msg)
                    {
                      char msg_next[BUFLEN] = "MSG#";
                      char temp[BUFLEN];
                      sprintf(temp,"%d",next->seq_id);
                      strcat(msg_next,temp);
                      strcat(msg_next,"#");
                      sprintf(temp,"%d",next->client_id);
                      strcat(msg_next,temp);
                      strcat(msg_next,"#");
                      sprintf(temp,"%d",next->msg_id);
                      strcat(msg_next,temp);
                      strcat(msg_next,"#");
                      strcat(msg_next,next->msg);
                      multicast(socket,msg_next);
                      client_list[idx].last_msg_id = next->msg_id;
                      TAILQ_REMOVE(&message_head,next,entries);
                      free(next);
                      multicast(socket,msg);
                      client_list[idx].last_msg_id = item->msg_id;
                      TAILQ_REMOVE(&message_head,item,entries);
                      free(item);
                      flag = 1;
                      //break;          //IS THIS NECESSARY?

                    }
                  }
                }

                /*
                IF NEXT MESSAGE TO BE SENT IS NOT FOUND IN QUEUE, PUSH TOP MESSAGE TO END OF QUEUE
                */

                if(flag == 0)
                { 
                  struct message *last;
                  last->seq_id = item->seq_id;
                  last->client_id = item->client_id;
                  last->msg_id = item->msg_id;
                  strcpy(last->msg,item->msg);
                  TAILQ_INSERT_TAIL(&message_head,last,entries);
                  TAILQ_REMOVE(&message_head,item,entries);
                  free(item);
                }
              } // end of if (finding the right client structure)
            }
          }   // end of for (looping through id array to find the existing clients)

      } // end of foreach (traversing through the message queue)
    }
  } // end of while
}


void* message_pinging()
{

    /*
        RESPOND TO THE ELECTION ALGORITHM PINGING IT
     */

  struct sockaddr_in seq,client;
  int s, n, len = sizeof(client);
  char buf[BUFLEN], ping_back[BUFLEN] = "I AM ALIVE";;
    
  if((s = socket(PF_INET, SOCK_DGRAM, 0)) < 0) {
      perror("Socket error");
      exit(-1);
   }

   seq.sin_family = AF_INET;
   seq.sin_port = htons(PORT_PING);
   seq.sin_addr.s_addr = htonl(INADDR_ANY);

  if(bind(s, (struct sockaddr*)&seq, sizeof(seq)) < 0){
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

    if(strcmp("PING",buf)==0)
     {
       
       if((sendto(s,ping_back,BUFLEN,0,(struct sockaddr *)&client, sizeof(client))) < 0)
       {
          perror("Ping Back Error");
          exit(-1);
       }

     }
   }
}


int main(int argc, char *argv[]){
   struct sockaddr_in server;
   int s;
    
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


   /*

    Creating two threads to handle message receiving and multicasting simultaneously

   */

   pthread_t p1,p2,p3;

  if(pthread_create(&p1, NULL, message_receiving, s))
    {
    printf("message_receiving thread failed \n");
    exit(-1);
    }
    

  if(pthread_create(&p2, NULL, message_multicasting, s))
    {
    printf("message_multicasting thread failed \n");
    exit(-1);
    }
  
  if(pthread_create(&p3, NULL, message_pinging, NULL))
    {
    printf("PINGING thread failed \n");
    exit(-1);
    }

}