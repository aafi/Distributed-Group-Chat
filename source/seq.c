
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
#define PORT_ELE 8174
#define NPACK 10
#define BUFLEN 1024
#define MAX 15
#define TIMEOUT_SEC 3
#define TIMEOUT_USEC 0


int msg_seq_id = 0;
int hb_counter = 0, num_client_hb = -1;
int holdback = 0;

pthread_mutex_t client_lock;
pthread_mutex_t message_lock;
pthread_mutex_t counter_lock;

struct client{
   char ip[BUFLEN];
   char name[BUFLEN];
   int port;
   int client_id;
   int last_msg_id;     //id of the last message sent by the client
   int leader;
   int counter;
   double time_of_join;

   TAILQ_ENTRY(client) entries;

};

//This is the head of the TAILQ for messages

TAILQ_HEAD(,client) client_head;

struct message{
   int seq_id;
   int client_id;
   int msg_id;
   char msg[BUFLEN];
   //int ack_vector[MAX];
   int counter;
   int sent;

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

   struct client *item,*temp_item;

   if(!TAILQ_EMPTY(&client_head))
   {
      
    for(item=TAILQ_FIRST(&client_head);item!=NULL;item=TAILQ_NEXT(item,entries))
   {
         //temp_item = TAILQ_NEXT(item,entries);
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

void multicast_ea(int socket,char * msg)
{
   int idx = 0;
   //printf("Inside");

   struct client *item,*temp_item;
   pthread_mutex_lock(&client_lock);

   if(!TAILQ_EMPTY(&client_head))
   {
   for(item=TAILQ_FIRST(&client_head);item!=NULL;item=TAILQ_NEXT(item,entries))
   {
         // temp_item = TAILQ_NEXT(item,entries);
         struct sockaddr_in clnt;
         clnt.sin_family = AF_INET;
         clnt.sin_port = htons(PORT_ELE);
         clnt.sin_addr.s_addr = inet_addr(item->ip);
        
         if((sendto(socket,msg,BUFLEN,0,(struct sockaddr *)&clnt, sizeof(clnt))) < 0)
         {
            perror("Broadcast Error");
            exit(-1);
         }
          
    }
  } 
  pthread_mutex_unlock(&client_lock);
}

struct timeval get_current_time()
{
  struct timeval tv;
  gettimeofday(&tv,NULL);
  return tv;
}

void multicast_clist(int socket)
{
     
     // printf("inside multicast clist\n");
     char multi[BUFLEN] = "SEQ#CLIENT#INFO#";
     char temp[BUFLEN];
     int num_client = count_clients();
     sprintf(temp,"%d",num_client);
     strcat(multi,temp);

    // pthread_mutex_lock(&client_lock);

     if(!TAILQ_EMPTY(&client_head))
     {
        struct client *item_client,*temp_item;
        
        for(item_client = TAILQ_FIRST(&client_head);item_client!=NULL;item_client = TAILQ_NEXT(item_client,entries))
        {
           // temp_item = TAILQ_NEXT(item_client,entries);
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
           sprintf(temp,"%d",item_client->last_msg_id);
           strcat(multi,"#");
           strcat(multi,temp);
           
        }

     }

    
    multicast(socket,multi);
   
}


int count_clients()
{
  int num_client = 0;
  struct client *item_client,*temp;
  if(!TAILQ_EMPTY(&client_head))
   {
    for(item_client = TAILQ_FIRST(&client_head); item_client!=NULL;item_client = TAILQ_NEXT(item_client,entries))
    {
      num_client++;
    }
   } 
   
  return num_client;
}


int requestid(char * ip, int port, char * name)
{
   int client_id = 0;
   struct client *c,*item,*temp_item;
   c = malloc(sizeof(*c));
   int i,flag;

   pthread_mutex_lock(&client_lock);

   if(!TAILQ_EMPTY(&client_head))
   { 
    for(i=0;i<=count_clients();i++)
    {
      client_id = i;
      flag = 0;
    
    for(item = TAILQ_FIRST(&client_head);item!=NULL;item = TAILQ_NEXT(item,entries))
     {
        
        if(item->client_id == client_id)
        {
          flag = 1;
          break;
        }
        
     }
     if(flag == 0)
      break;
    }
   }
   else
   {
    client_id = 0;
   }

   pthread_mutex_unlock(&client_lock);

    strcpy(c->ip,ip);
    c->port = port;
    strcpy(c->name,name);
    c->last_msg_id = -1;
    c->client_id = client_id;
    pthread_mutex_lock(&client_lock);
    int num = count_clients();
    pthread_mutex_unlock(&client_lock);
    if(num == 1)
      c->leader = 1;
    else
      c->leader = 0;

    c->counter = 0;

    struct timeval join_time = get_current_time();
    c->time_of_join = join_time.tv_sec + (join_time.tv_usec/1000000);

    // printf("Client %s joined as client %d \n",c->name,c->client_id);
    // printf("BEFORE INSERT\n");
    pthread_mutex_lock(&client_lock);
    // printf("INSERT LOCK\n");
    TAILQ_INSERT_TAIL(&client_head,c,entries);
    pthread_mutex_unlock(&client_lock);

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

  pthread_mutex_lock(&message_lock);

  for(item = TAILQ_FIRST(&message_head);item!=NULL;item=tmp_item)
  {
    tmp_item = TAILQ_NEXT(item,entries);
      

    /* This means all the clients have received the message */
    if(item->counter <= 0)
    { 
      
      int client_id = item->client_id;

      pthread_mutex_lock(&client_lock);

      if(!TAILQ_EMPTY(&client_head))
      {
        struct client *item_client,*client_next;
        //TAILQ_FOREACH(item_client,&client_head,entries)
        for(item_client = TAILQ_FIRST(&client_head);item_client!=NULL;item_client = TAILQ_NEXT(item_client,entries))
        {
          // client_next = TAILQ_NEXT(item_client,entries);
          if(item_client->client_id == client_id)
          {
            struct sockaddr_in clnt;
            clnt.sin_family = AF_INET;
            clnt.sin_port = htons(item_client->port);
            clnt.sin_addr.s_addr = inet_addr(item_client->ip);
            char msg[BUFLEN] = "SEQ#REM#", msg_hb[BUFLEN] = "SEQ#REMHB#",temp[BUFLEN];
            sprintf(temp,"%d",item->msg_id);
            strcat(msg,temp);
            sprintf(temp,"%d",item->seq_id);
            strcat(msg_hb,temp);
        
            //printf("Telling client to remove: %s\n", item->msg);
            if((sendto(s,msg,BUFLEN,0,(struct sockaddr *)&clnt, sizeof(clnt))) < 0)
            {
              perror("Send Error");
              exit(-1);
            }

            //Remove message from hold back queue
            multicast(s,msg_hb);

          }
        }
      } 

      pthread_mutex_unlock(&client_lock);

      
      TAILQ_REMOVE(&message_head,item,entries);
      // printf("Message to be removed: %s \n",item->msg);
      free(item);
      
    }
  }



  if(TAILQ_EMPTY(&message_head) && (hb_counter == num_client_hb) )
  {
   // printf("inside sendall condition\n");
    char temp[BUFLEN] = "SEQ#SENDALL";
    pthread_mutex_lock(&client_lock);
    multicast(s,temp);
    pthread_mutex_unlock(&client_lock);
    hb_counter = -1;
  }

  pthread_mutex_unlock(&message_lock);
}


void* message_receiving(int s)
{
  char * tok[BUFLEN];
  struct sockaddr_in client;
  int n, len = sizeof(client);
  char buf[BUFLEN],reply[BUFLEN],buf_copy[BUFLEN];
  const char * temp;
  int socket = s; 

  while(1)
   {

      if((n = recvfrom(socket, buf, BUFLEN, 0,(struct sockaddr*)&client, &len)) < 0)
      {
         perror("Receive Error");
         exit(-1);
      } 

      if(holdback==1)
      {
        if(hb_counter == num_client_hb)
        {
          pthread_mutex_lock(&message_lock);
          struct message *tmp;
          for(tmp=TAILQ_FIRST(&message_head);tmp!=NULL;tmp=TAILQ_NEXT(tmp,entries))
          {
            if(tmp->seq_id < msg_seq_id)
              tmp->counter = 0;
          }

          pthread_mutex_unlock(&message_lock);
        }
      }

    strcpy(buf_copy,buf);

      char * token;
      token = strtok(buf,"#");

      if (strcmp("REQUEST",token)==0)
      {
         //printf("JOIN MESSAGE : %s\n", buf_copy);
         int seq;
         int i = 0;
         while(token !=NULL)
         {
            token = strtok(NULL,"#");
            tok[i] = token;
            i++;
         }

         pthread_mutex_lock(&client_lock);
         int num = count_clients();
         pthread_mutex_unlock(&client_lock);

         if(num == MAX)
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
         

         pthread_mutex_lock(&client_lock);
         multicast_clist(socket);
         pthread_mutex_unlock(&client_lock);
                  
         char status[BUFLEN] = "SEQ#STATUS#";
         char status_msg[BUFLEN];
         sprintf(status_msg,"NOTICE %s joined on %s:%s",tok[2],tok[0],tok[1]);
         strcat(status,status_msg);

         pthread_mutex_lock(&client_lock);
         multicast(socket,status);
         pthread_mutex_unlock(&client_lock);

         // printf("NUMBER OF CLIENTS IN THE SYSTEM: %d\n",count_clients());

       
      }

      else if (strcmp("MESSAGE",token)==0)
      {
         
         int i = 0;
         
         char *tok[BUFLEN];
         detokenize(buf_copy,tok,"#");
         
         struct message *item;
         item = malloc(sizeof(*item));

         if(item!=NULL)
         {
         item->client_id = atoi(tok[1]);
         item->msg_id = atoi(tok[2]);
         strcpy(item->msg,tok[3]);
         item->seq_id = -1;
        }
        

         
         pthread_mutex_lock(&client_lock);

         item->counter = count_clients();

         pthread_mutex_unlock(&client_lock);

         item->sent = 0;

         /*
                 * Add our item to the end of tail queue. The first
                 * argument is a pointer to the head of our tail
                 * queue, the second is the item we want to add, and
                 * the third argument is the name of the struct
                 * variable that points to the next and previous items
                 * in the tail queue.
         */


        pthread_mutex_lock(&message_lock);

         TAILQ_INSERT_TAIL(&message_head,item,entries);

        pthread_mutex_unlock(&message_lock);

         
         // DEBUG
         // struct client *item_client,*temp_client;

         // pthread_mutex_lock(&client_lock);
         // for(item_client=TAILQ_FIRST(&client_head);item_client!=NULL;item_client=TAILQ_NEXT(item_client,entries))
         // {
         //    // temp_client = TAILQ_NEXT(item_client,entries);
         //    if(item_client->client_id == item->client_id)
         //      printf("FOR CLIENT %d : LAST MSG ID %d\n",item_client->client_id,item_client->last_msg_id);
         // }

         // pthread_mutex_unlock(&client_lock);

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
         
         char * ack[BUFLEN];
         detokenize(buf_copy,ack,"#");

         struct message *item,*temp_item;

         pthread_mutex_lock(&message_lock);
         if(!TAILQ_EMPTY(&message_head))
         {
          for(item=TAILQ_FIRST(&message_head);item!=NULL;item = TAILQ_NEXT(item,entries))
           {
            if(atoi(ack[2]) == item->seq_id)
            {
              item->counter--;

            }
          }
      }
      pthread_mutex_unlock(&message_lock);
     }

     else if(strcmp("LOST",token)==0)
     {
        // printf("LOST MESSAGE REQUEST: %s\n", buf);
        token = strtok(NULL,"#");
        int lost_msg_id = atoi(token);
        //printf("Lost msg id : %d \n",lost_msg_id);
        char msg[BUFLEN] = "MSG#";
        char temp[BUFLEN];

        pthread_mutex_lock(&message_lock);

        if(!TAILQ_EMPTY(&message_head))
        {
          struct message *item,*temp_item;
          for(item=TAILQ_FIRST(&message_head); item!=NULL; item = TAILQ_NEXT(item,entries))
          {
            // temp_item = TAILQ_NEXT(item,entries);
            // printf("current message being checked: %d \n",item->seq_id);
            if(lost_msg_id == item->seq_id)
            {
            //  printf("Found correct message %d",lost_msg_id);         
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
              // printf("SENDING LOST MSG: %s\n",msg);
              if((sendto(socket,msg,BUFLEN,0,(struct sockaddr *)&client, sizeof(client))) < 0)
              {
                perror("Lost Message Sending Error");
                exit(-1);
              }
              
            }
          }
        }
        pthread_mutex_unlock(&message_lock);
        
     }

     else if(strcmp("HB",token)==0)
     {
        holdback = 1;
        // printf("SEQUENCER hb msg: %s\n", buf_copy);
        hb_counter++;
        char * hb[BUFLEN];
        detokenize(buf_copy,hb,"#");
        int flag = 0;
        
        int client_id = atoi(hb[1]);
        //int flag = 0;

        if(msg_seq_id == 0)
          {
            msg_seq_id = atoi(hb[2]);
           
           // flag = 1;
          }
        else if(atoi(hb[2])<msg_seq_id)
          {
            msg_seq_id = atoi(hb[2]);
          //  flag = 1;
          }

        if(hb_counter == num_client_hb)
        {
          pthread_mutex_lock(&message_lock);
          struct message *tmp;
          for(tmp=TAILQ_FIRST(&message_head);tmp!=NULL;tmp=TAILQ_NEXT(tmp,entries))
          {
            if(tmp->seq_id < msg_seq_id)
              tmp->counter = 0;
          }

          pthread_mutex_unlock(&message_lock);
        }

                
        int count = (atoi(hb[3])*4)+4;

        int idx = 4;
        pthread_mutex_lock(&message_lock);
        for(idx;idx < count; idx+=4)
        {
          flag = 0;
          
          if(!TAILQ_EMPTY(&message_head))
          { 
            struct message *item,*temp_item;
            for(item=TAILQ_FIRST(&message_head);item!=NULL;item=TAILQ_NEXT(item,entries))
            {
              if(item->seq_id == atoi(hb[idx]))
                {

                  item->counter--;
                  flag = 1;
                }
            }
          }
          
          if(flag == 0)
          {
            struct message *msg;
            int num;
            msg = malloc(sizeof(*msg));
            msg->seq_id = atoi(hb[idx]);
            msg->client_id = atoi(hb[idx+1]);
            msg->msg_id = atoi(hb[idx+2]);
            strcpy(msg->msg,hb[idx+3]);

            pthread_mutex_lock(&client_lock);
            num = count_clients();
            pthread_mutex_unlock(&client_lock);

            msg->counter = num;
            msg->counter--;
            msg->sent = 0;

            
              TAILQ_INSERT_TAIL(&message_head,msg,entries);
          

         }
          
        }
        pthread_mutex_unlock(&message_lock);

        if(hb_counter == num_client_hb)
          holdback = 0;

     }

   }

}




void* message_multicasting(int s)
{
  int socket = s;
  int count = 0;
  while(1)
  {
    
    if(holdback == 0)
    {
    msg_removal(socket); 

    pthread_mutex_lock(&message_lock);

    if(!TAILQ_EMPTY(&message_head))
    {         
      
      struct message *item,*tmp_item;

      item = TAILQ_FIRST(&message_head);
      while(item!=NULL){
        tmp_item = TAILQ_NEXT(item, entries);
        if(item->sent == 1)
          {
            item = tmp_item;
          }
         else
          {
            int idx = 0, flag = 0;

            pthread_mutex_lock(&client_lock);

            if(!TAILQ_EMPTY(&client_head))
            {
              struct client *item_client,*tmp_client;
              for(item_client=TAILQ_FIRST(&client_head);item_client!=NULL;item_client=TAILQ_NEXT(item_client,entries))
              {
                // tmp_client = TAILQ_NEXT(item_client,entries);
                /*
                Finding the right client structure
                */
                    if(item->client_id == item_client->client_id)
                    {
                      // printf("Found Right client structure \n");
                        
                        
                    /*
                    CHECK IF THE MESSAGE AT THE TOP IS THE ONE TO BE SENT NEXT
                    */

                      int next_msg = item_client->last_msg_id+1;

                      // printf("CLIENT %d : MSG to be sent %d \n",item_client->client_id,next_msg);

                      if(item->msg_id == next_msg)
                      {

                        // printf("SEQUENCER: MESSAGE %d FOUND TOP OF THE QUEUE\n",item->msg_id);
                        char msg[BUFLEN] = "MSG#";
                        char temp[BUFLEN];

                        //assigning global sequence id
                        item->seq_id = msg_seq_id++;

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

                        item->sent = 1;
                        multicast(socket,msg);

                        item_client->last_msg_id = item->msg_id;
                        // printf("CLIEND ID: %d ... MESSAGE ID : %d\n",item_client->client_id,item_client->last_msg_id);
                        flag = 1;
                      }

                      /*
                      TRAVERSE THROUGH THE LIST TO FIND IF THE NEXT MESSAGE TO BE SENT EXISTS
                      */

                      else
                      { 
                        struct message *next,*tmp_next;
                        for(next=TAILQ_FIRST(&message_head);next!=NULL;next=TAILQ_NEXT(next,entries))
                        {
                          if(next->msg_id == next_msg)
                          {
                            // printf("SEQUENCER: MESSAGE %d FOUND LATER IN THE QUEUE\n",next->msg_id);
                            char msg_next[BUFLEN] = "MSG#";
                            char temp[BUFLEN];

                            //assigning global sequence id
                            next->seq_id = msg_seq_id++;

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
                            next->sent = 1;
                            multicast(socket,msg_next);
                            item_client->last_msg_id = next->msg_id;
                            // printf("CLIEND ID: %d ... MESSAGE ID : %d\n",item_client->client_id,item_client->last_msg_id);
                          }
                        }
                      }

                      /*
                      IF NEXT MESSAGE TO BE SENT IS NOT FOUND IN QUEUE, PUSH TOP MESSAGE TO END OF QUEUE
                      */

                      if((flag == 0) && (item->sent == 0))
                      { 
                        struct message *last;
                        last = malloc(sizeof(*last));
                        last->seq_id = item->seq_id;
                        last->client_id = item->client_id;
                        last->msg_id = item->msg_id;
                        last->counter = item->counter;
                        last->sent = item->sent;
                        strcpy(last->msg,item->msg);
                        TAILQ_INSERT_TAIL(&message_head,last,entries);
                        TAILQ_REMOVE(&message_head,item,entries);
                        free(item);
                      }

                    } // end of if (finding the right client structure)
                 
                }   // end of for (looping through id array to find the existing clients)

            } // end of foreach (traversing through the message queue)
               
            pthread_mutex_unlock(&client_lock);
        }
        item = tmp_item;
      }
    }
 // nanosleep((struct timespec[]){{0,100000000}},NULL);

    pthread_mutex_unlock(&message_lock);
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

  

  while(1)
  {
    
    int msec = 0, trigger = 2000;
    clock_t before = clock();
    int flag;
    do
    {
      

      if((n = recvfrom(s, buf, BUFLEN, 0,(struct sockaddr*)&client_in, &len)) < 0)
        {
           perror("Receive Error Ping");
           exit(-1);
        }

       

      char * token;
      token = strtok(buf,"#");

      if(strcmp("PING",token)==0)
       {
         
         token = strtok(NULL,"#");
         if(!TAILQ_EMPTY(&client_head))
         {
          struct client *item_client,*tmp_clnt;
          // TAILQ_FOREACH(item_client,&client_head,entries)
          for(item_client = TAILQ_FIRST(&client_head);item_client!=NULL;item_client=TAILQ_NEXT(item_client,entries))
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
      //printf("Number of pings from client: %d\n", item_client -> counter);
      struct timeval curr_time = get_current_time();
      double t2 = curr_time.tv_sec + ( curr_time.tv_usec / 1000000 );

      if((t2-item_client->time_of_join) > 2)
      {
        if(item_client->counter<5)
        {
         
              char status[BUFLEN] = "SEQ#EXIT#";
              char tmp[BUFLEN];
              sprintf(tmp,"%d",item_client->client_id);
              strcat(status,tmp);
              strcat(status,"#");
              char status_msg[BUFLEN];
              sprintf(status_msg,"NOTICE %s left the chat or crashed",item_client->name);
              strcat(status,status_msg);
              multicast(sock,status);

              TAILQ_REMOVE(&client_head,item_client,entries);
              free(item_client);
              multicast_clist(sock);
              if(holdback==1)
              {
                num_client_hb--;
              }
            

        }
        else
        {
          item_client->counter = 0;
        }
      }
      
     }
   }
    
 }
}


int main(int argc, char *argv[]){
   struct sockaddr_in server,client_in,client_out;
   char buf[BUFLEN];
   int s,n, len = sizeof(client_in);
   char * tok[BUFLEN];

   if(pthread_mutex_init(&client_lock,NULL)!=0)
   {
    printf("CLIENT LOCK FAIL");
    exit(-1);
   }

   if(pthread_mutex_init(&message_lock,NULL)!=0)
   {
    printf("MESSAGE LOCK FAIL");
    exit(-1);
   }

   if(pthread_mutex_init(&counter_lock,NULL)!=0)
   {
    printf("MESSAGE LOCK FAIL");
    exit(-1);
   }
    
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

    // printf("AFTER ELECTION/NEW CHAT: %s\n",buf);   

    detokenize(buf,tok,"#"); 

    /* Initialize the tail queue */

    TAILQ_INIT(&message_head); 
    TAILQ_INIT(&client_head);


    if (strcmp("NEWLEADER",tok[0]) == 0)
    {
      char notice[BUFLEN] = "SEQ#STATUS#";
      int i = 3, num_clients = (atoi(tok[1])*5)+3;
      num_client_hb = atoi(tok[1]);
      msg_seq_id = 0;
      strcat(notice,tok[2]);

      for(i;i<num_clients;i+=5)
      {
        struct client *c;
        c = malloc(sizeof(*c));
        strcpy(c->ip,tok[i]);
        c->port = atoi(tok[i+1]);
        strcpy(c->name,tok[i+3]);
        c->last_msg_id = atoi(tok[i+4]);
        
        c->client_id = atoi(tok[i+2]);
        if(strcmp(my_ip_addr,tok[i]) == 0)
          c->leader = 1;
        else
          c->leader = 0;

        
        c->counter = 0;
        struct timeval curr_time = get_current_time();
        c->time_of_join = curr_time.tv_sec + ( curr_time.tv_usec / 1000000 );

        pthread_mutex_lock(&client_lock);
        TAILQ_INSERT_TAIL(&client_head,c,entries);
        pthread_mutex_unlock(&client_lock);
      }
      
      // printf("Win-broadcas: %s\n",win_broadcast);
      // printf("notice message : %s\n",notice);
      multicast(s,win_broadcast);
      multicast_ea(s,win_broadcast);

      multicast(s,notice);
    } 
    
    multicast_clist(s);

    //REQUEST the last messages from each client
     
    char req[BUFLEN] = "SEQ#HB";
    multicast(s,req);  

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