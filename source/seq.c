
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/queue.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PORT 5678
#define PORT_PING 5679
#define PORT_PARENT 1705
#define NPACK 10
#define BUFLEN 1024
#define MAX 15
#define TIMEOUT_SEC 3
#define TIMEOUT_USEC 0

// int id[MAX] = {0};
int msg_seq_id = 0;

struct client{
   char ip[BUFLEN];
   char name[BUFLEN];
   int port;
   int client_id;
   int last_msg_id;     //id of the last message sent by the client
   int leader;
   int counter;

   TAILQ_ENTRY(client) entries;

};//client_list[MAX];

//This is the head of the TAILQ for messages

TAILQ_HEAD(,client) client_head;

struct message{
   int seq_id;
   int client_id;
   int msg_id;
   char msg[BUFLEN];
   int ack_vector[MAX];

   /*
         * This holds the pointers to the next and previous entries in
         * the tail queue.
    */

   TAILQ_ENTRY(message) entries;  
};

//This is the head of the TAILQ for messages

TAILQ_HEAD(,message) message_head;


/*
  
   Function to multicast a message to all clients available
      
*/


void multicast(int socket,char * msg)
{
   int idx = 0;
   //printf("Inside");

   struct client *item;
   if(!TAILQ_EMPTY(&client_head))
   {
      TAILQ_FOREACH(item, &client_head, entries)
   {
         struct sockaddr_in clnt;
         clnt.sin_family = AF_INET;
         clnt.sin_port = htons(item->port);
         clnt.sin_addr.s_addr = inet_addr(item->ip);
        
         if((sendto(socket,msg,BUFLEN,0,(struct sockaddr *)&clnt, sizeof(clnt))) < 0)
         {
            perror("Broadcast Error");
            exit(-1);
         }
         
    }
  } 
}

void tokenize_client(int socket)
{
     
     char multi[BUFLEN] = "SEQ#CLIENT#INFO#";
     char temp[BUFLEN];
     int num_client = count_clients();
     sprintf(temp,"%d",num_client);
     strcat(multi,temp);


     if(!TAILQ_EMPTY(&client_head))
     {
        struct client *item_client;
        TAILQ_FOREACH(item_client,&client_head,entries)
        {
           strcat(multi,"#");
           strcat(multi,item_client->ip);
           strcat(multi,"#");
           sprintf(temp,"%d",item_client->port);
           strcat(multi,temp);
           strcat(multi,"#");
           sprintf(temp,"%d",item_client->client_id);
           strcat(multi,temp);
           strcat(multi,"#");
           strcat(multi,item_client->name);
           // strcat(multi,"#");
           // sprintf(temp,"%d",item_client->leader);
           // strcat(multi,temp);

        }

     }

    multicast(socket,multi);

}


int count_clients()
{
  int num_client = 0;
  struct client *item_client;
  if(!TAILQ_EMPTY(&client_head))
   {
    TAILQ_FOREACH(item_client, &client_head, entries)
      num_client++;
   } 
  return num_client;
}


int requestid(char * ip, int port, char * name)
{
   int client_id = 0;
   struct client *c,*item;
   c = malloc(sizeof(*c));

   if(!TAILQ_EMPTY(&client_head))
   { 
    TAILQ_FOREACH(item,&client_head,entries)
     {
        if(item->client_id == client_id)
        {
          client_id++;
        }
        else
          break;
     }
   }
   else
   {
    client_id = 0;
   }

    strcpy(c->ip,ip);
    c->port = port;
    strcpy(c->name,name);
    c->last_msg_id = -1;
    c->client_id = client_id;
    int num = count_clients();
    if(num == 1)
      c->leader = 1;
    else
      c->leader = 0;

    c->counter = 0;
   
    TAILQ_INSERT_TAIL(&client_head,c,entries);
    
    return client_id;
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


void msg_removal(int s)
{
  
  int idx;
  struct message *item, *tmp_item;
  for(item = TAILQ_FIRST(&message_head);item!=NULL;item=tmp_item)
  {
    tmp_item = TAILQ_NEXT(item,entries);
    int flag = 0;
    for(idx=0;idx<MAX;idx++)
    {
      if(item->ack_vector[idx] == 1)
          {
            flag = 1; 
            //return -1;
          }
    }
    

    /* This means all the clients have received the message */
    if(flag == 0)
    { 
      int client_id = item->client_id;
      if(!TAILQ_EMPTY(&client_head))
      {
        struct client *item_client;
        TAILQ_FOREACH(item_client,&client_head,entries)
        {
          if(item_client->client_id == client_id)
          {
            struct sockaddr_in clnt;
            clnt.sin_family = AF_INET;
            clnt.sin_port = htons(item_client->port);
            clnt.sin_addr.s_addr = inet_addr(item_client->ip);
            char msg[BUFLEN] = "SEQ#REM#",temp[BUFLEN];
            sprintf(temp,"%d",item->msg_id);
            strcat(msg,temp);
        
            //printf("Telling client to remove: %s\n", item->msg);
            if((sendto(s,msg,BUFLEN,0,(struct sockaddr *)&clnt, sizeof(clnt))) < 0)
            {
              perror("Send Error");
              exit(-1);
            }
          }
        }
      } 
      

     // printf("Message to be removed: %s \n",item->msg);
      TAILQ_REMOVE(&message_head,item,entries);
      free(item);
      
    }
  }
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

         if(count_clients() == MAX)
         {
          strcpy(reply,"FAILURE");
         }
         else
         {
         seq = requestid(tok[0],atoi(tok[1]),tok[2]);   // Gets back a sequence number for the new client
         char tmp[BUFLEN];
         sprintf(tmp, "%d", seq);
         strcpy(reply,"SUCCESS#");
         strcat(reply,tmp);
         strcat(reply,"#");
         sprintf(tmp, "%d", msg_seq_id);
         strcat(reply,tmp);
         }

         if((sendto(socket,reply,sizeof(reply),0,(struct sockaddr*)&client, sizeof(client))) < 0)    //send reply back
         {
            perror("Send Error");
            exit(-1);
         }

         
         tokenize_client(socket);
         //multicast(socket,multi);
         
         char status[BUFLEN] = "SEQ#STATUS#";
         char status_msg[BUFLEN];
         sprintf(status_msg,"NOTICE %s joined on %s:%s",tok[2],tok[0],tok[1]);
         strcat(status,status_msg);
         multicast(socket,status);

       
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
         item = malloc(sizeof(*item));
         item->client_id = atoi(tok[0]);
         item->msg_id = atoi(tok[1]);
         strcpy(item->msg,tok[2]);
         item->seq_id = msg_seq_id++;

         int id[MAX] = {0};
         if(!TAILQ_EMPTY(&client_head))
         {
          struct client *c;
          TAILQ_FOREACH(c,&client_head,entries)
          {
            id[c->client_id] = 1;
          }
         }


         int idx = 0;

         for(idx;idx<MAX;idx++)
         {
          item->ack_vector[idx] = id[idx];
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

         /*
            Send acknowledgement back to the client that message has been received and put in the queue
          
         */

        char ack[BUFLEN] = "SEQ#ACK#";
        strcat(ack,tok[1]);


        if((sendto(socket,ack,BUFLEN,0,(struct sockaddr *)&client, sizeof(client))) < 0)
       {
          perror("Acknowledgement Error");
          exit(-1);
       }
   
     }

     else if(strcmp("ACK",token) == 0)
     {
         int i = 0;
         while(token!=NULL)
         {  
            
            token = strtok(NULL,"#");
            tok[i] = token;
            i++;
         }

         struct message *item;
         TAILQ_FOREACH(item, &message_head, entries)
         {
          if(atoi(tok[1]) == item->seq_id)
          {
            int client_id = atoi(tok[0]);
            item->ack_vector[client_id] = 2;
            break;
          }
        }
     }

   }

}




void* message_multicasting(int s)
{
  int socket = s;
  int count = 0;
  while(1)
  {
    msg_removal(socket); 
    if(!TAILQ_EMPTY(&message_head))
    {         
      
      struct message *item,*tmp_item;
      item = TAILQ_FIRST(&message_head);
      int idx = 0, flag = 0;
      if(!TAILQ_EMPTY(&client_head))
      {
        struct client *item_client;
        TAILQ_FOREACH(item_client,&client_head,entries)
        {
          /*
          Finding the right client structure
          */

              if(item->client_id == item_client->client_id)
              {
                 // printf("Found client structure \n");
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

                int next_msg = item_client->last_msg_id+1;

                if(item->msg_id == next_msg)
                {
                  multicast(socket,msg);
                  item_client->last_msg_id = item->msg_id;
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
                      item_client->last_msg_id = next->msg_id;
                    }
                  }
                }

                /*
                IF NEXT MESSAGE TO BE SENT IS NOT FOUND IN QUEUE, PUSH TOP MESSAGE TO END OF QUEUE
                */

                if(flag == 0)
                { 
                  struct message *last;
                  last = malloc(sizeof(*last));
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

      //} // end of foreach (traversing through the message queue)
    }
  } // end of while
}


void* message_pinging(int sock)
{

    /*
        RESPOND TO THE ELECTION ALGORITHM PINGING IT
     */

  struct sockaddr_in seq,client_in,client_out;
  int s, n, len = sizeof(client_in),len_out=sizeof(client_out);
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

  struct timeval tv;
  tv.tv_sec = TIMEOUT_SEC;
  tv.tv_usec = TIMEOUT_USEC;


  while(1)
  {

    int msec = 0, trigger = 10;
    clock_t before = clock();
    do
    {
      if((n = recvfrom(s, buf, BUFLEN, 0,(struct sockaddr*)&client_in, &len)) < 0)
        {
           perror("Receive Error");
           exit(-1);
        }

      printf("%s\n",buf); //DEEPTI DEBIGGING

      char * token;
      token = strtok(buf,"#");
      printf("%s\n", token); //DEEPTI DEBUGGING

      if(strcmp("PING",token)==0)
       {
        
         printf("reached if \n");
         token = strtok(NULL,"#");
         if(!TAILQ_EMPTY(&client_head))
         {
          struct client *item_client;
          TAILQ_FOREACH(item_client,&client_head,entries)
          {
            if(item_client->client_id == atoi(token))
            {
              item_client->counter++;
              break;
            }
          }
         }
         
         if((sendto(s,ping_back,BUFLEN,0,(struct sockaddr *)&client_in, sizeof(client_in))) < 0)
         {
            perror("Ping Back Error");
            exit(-1);
         }

       }

       clock_t difference = clock() - before;
       msec = difference*1000/CLOCKS_PER_SEC;

   }while(msec<trigger);

   if(!TAILQ_EMPTY(&client_head))
   {
    struct client *item_client,*tmp_item;
    for(item_client = TAILQ_FIRST(&client_head);item_client!=NULL;item_client=tmp_item)
    {
      tmp_item = TAILQ_NEXT(item_client,entries);
      if(item_client->counter<10)
      {
        char req_status[BUFLEN] = "SEQ#PING#STATUS";
        client_out.sin_family = AF_INET;
        client_out.sin_port = htons(item_client->port);
        client_out.sin_addr.s_addr = inet_addr(item_client->ip);

        if(sendto(s,req_status,BUFLEN,0,(struct sockaddr*)&client_out,sizeof(client_out))<0)
        {
            exit(-1);
        }

        if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
        {
            perror("Error");
        }

        if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr*)&client_out, &len_out) < 0)
        {
            char status[BUFLEN] = "SEQ#STATUS#";
            char status_msg[BUFLEN];
            sprintf(status_msg,"NOTICE %s left the chat or crashed",item_client->name);
            strcat(status,status_msg);
            multicast(sock,status);

            TAILQ_REMOVE(&client_head,item_client,entries);
            free(item_client);

        }

      }
     }
   }

  //char * multi[BUFLEN] = "SEQ#CLIENT#INFO#";
  tokenize_client(sock);
  //multicast(socket,multi);
 
 }
}


int main(int argc, char *argv[]){
   struct sockaddr_in server,client_in,client_out;
   char buf[BUFLEN];
   int s,n, len = sizeof(client_in);
   char * tok[BUFLEN];
    
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
 
   char win_broadcast[BUFLEN]="SEQ#EA#";
   char tmp[BUFLEN];

   const char* my_ip_addr = get_ip_address();

   //update_clientlist(my_ip_addr,s);

   strcat(win_broadcast,my_ip_addr);
   strcat(win_broadcast,"#");
   sprintf(tmp,"%d",PORT);
   strcat(win_broadcast,tmp);


   // Sending the client that runs the sequencer the IP and Port info of the sequencer
    client_out.sin_family = AF_INET;
    client_out.sin_port = htons(PORT_PARENT);
    client_out.sin_addr.s_addr = inet_addr(my_ip_addr);

    if(sendto(s,win_broadcast,BUFLEN,0,(struct sockaddr*)&client_out,sizeof(client_out))<0)
    {
        exit(-1);
    }

    if((n = recvfrom(s, buf, BUFLEN, 0,(struct sockaddr*)&client_in, &len)) < 0)
    {
       perror("Receive Error");
       exit(-1);
    }    

    detokenize(buf,tok,"#"); 

    /* Initialize the tail queue */

    TAILQ_INIT(&message_head); 
    TAILQ_INIT(&client_head);

    if (strcmp("NEWLEADER",tok[0]) == 0)
    {
      int i = 0, num_clients = atoi(tok[1]);
      for(i;i<num_clients;i+=4)
      {
        struct client *c;
        c = malloc(sizeof(*c));
        strcpy(c->ip,tok[i+2]);
        c->port = atoi(tok[i+3]);
        strcpy(c->name,tok[i+5]);
        c->last_msg_id = -1;
        c->client_id = atoi(tok[i+4]);
        if(strcmp(my_ip_addr,tok[i+2]) == 0)
          c->leader = 1;
        else
          c->leader = 0;

        c->counter = 0;
        TAILQ_INSERT_TAIL(&client_head,c,entries);
      }

      multicast(s,win_broadcast);
    } 


    
    tokenize_client(s);
     
       

   /*

    Creating three threads to handle message receiving, multicasting and pinging simultaneously

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
  
  if(pthread_create(&p3, NULL, message_pinging, s))
    {
    printf("PINGING thread failed \n");
    exit(-1);
    }

  pthread_join(p1,NULL);
  pthread_join(p2,NULL);
  pthread_join(p3,NULL);

}