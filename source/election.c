#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>

#define BUFLEN 1024
#define MAX 50
#define PORT 8174
#define TIMEOUT_SEC 3
#define TIMEOUT_USEC 0


struct election_process
{
    char ip[MAX];
    int port;
    int ele_id;
} ele_list[MAX];

struct client{
    char ip[BUFLEN];
    int port;
    int client_id;
   //int last_msg_id;     //id of the last message sent by the client
  // int leader;          //by default is 0. The client which is the leader will have 1
}client_list[MAX];

int total_clients = 3;
int curr_ele_id;
 
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

/*Description: This function takes a string (buf) and a token (token) and splits the string and places individual strings in token_result
*/
void detokenize(char buf[], char* token_result[], char* token)
{
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

void populate_clients()
{
	struct client c;

    strcpy(c.ip, "158.130.24.206");
    c.port = 1234;
	c.client_id = 1;

	client_list[0] = c;

	strcpy(c.ip, "158.130.24.207");
	c.port = 2345;
	c.client_id = 2;

	client_list[1] = c;

    strcpy(c.ip, "158.130.24.208");
    c.port = 3456;
    c.client_id = 3;

    client_list[2] = c;

}




int main(int argc, char** argv)
{
    struct sockaddr_in my_addr, serv_addr_seq, serv_addr_client, serv_addr_ele, serv_addr;
    int sockfd, i, election = 0;
    socklen_t slen=sizeof(serv_addr_seq);
    char buf[BUFLEN], temp[BUFLEN];
    char* token_result[BUFLEN];
    if(argc != 4)
    {
      printf("Usage : %s <Sequencer Server-IP> <sequencer port> <client_id> \n",argv[0]);
      exit(0);
    }
    
    populate_clients();
    //strcpy(buf, argv[3]);
    
    curr_ele_id = atoi(argv[3]);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0))==-1)
        err("socket");

    //calling_client_addr = argv[1]
    
    bzero(&my_addr, sizeof(my_addr));
    bzero(&serv_addr_seq, sizeof(serv_addr_seq));
    bzero(&serv_addr_client, sizeof(serv_addr_client));
    bzero(&serv_addr_ele, sizeof(serv_addr_ele));

    my_addr.sin_family = AF_INET;
    serv_addr_seq.sin_family = AF_INET;
    serv_addr_client.sin_family = AF_INET;
    serv_addr_ele.sin_family = AF_INET;

    serv_addr_seq.sin_port = htons(atoi(argv[2]));
    my_addr.sin_port = htons(PORT);
    serv_addr_ele.sin_port = htons(PORT);

    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1)
      err("bind");

    if (inet_aton(argv[1], &serv_addr_seq.sin_addr)==0)
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    printf("In election algorithm\n");

    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = TIMEOUT_USEC;

    while(1)
    {
    	
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
        {
            perror("Error");
        }
        
        strcpy(buf, "PING#");
        strcat(buf, argv[3]);
        send_msg(sockfd, buf, serv_addr_seq, slen); //PING SEQUENCER TO CHECK IF IT IS ACTIVE
        //printf("reached here\n");
        if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&serv_addr, &slen) < 0)
        {
            //TIMEOUT REACHED -> SEQUENCER IS NOT ACTIVE
            printf("timeout\n");
            send_msg(sockfd, "ARE YOU ALIVE?", serv_addr_seq, slen);
            if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&serv_addr, &slen) < 0) //DOUBLE CHECKING SEQ CRASH
            {

                begin_election:
                printf("2nd timeout: starting election\n");
                for (i = 0; i < total_clients; i++) //INFORMING ALL CLIENTS AND ELECTIONS THAT ELECTION IS BEING HELD
                {
                	serv_addr_client.sin_port = htons(client_list[i].port);

                    if (inet_aton(client_list[i].ip, &serv_addr_client.sin_addr)==0)
                    {
                        fprintf(stderr, "inet_aton() failed\n");
                        exit(1);
                    }
                    //printf("Inofrming: first inet_aton\n");
                    if (inet_aton(client_list[i].ip, &serv_addr_ele.sin_addr)==0)
                    {
                        fprintf(stderr, "inet_aton() failed\n");
                        exit(1);
                    }

                    //printf("informing: 2nd inet_aton\n");

                    if (client_list[i].client_id == curr_ele_id)
                    {
                        send_msg(sockfd, "ELECTION", serv_addr_client, slen);
                    }

                    else
                    {
                        send_msg(sockfd, "ELECTION", serv_addr_client, slen); //TELL CLIENTS TO STOP FOR ELECTION AND WAIT FOR NEW LEADER
                        //send_msg(sockfd, "ELECTION", serv_addr_ele, slen); //TELL ELECTIONS TO STOP PINGING AND PARTICIPATE IN ELECTION
                    }

                }

                for (i = 0; i < total_clients; i++) //SENDING ID TO ALL HIGHER ID ELECTIONS (MAKE THIS A FUNCTION??)
                {
                	//serv_addr_client.sin_port = htons(client_list[i].port);
                	//serv_addr_ele.sin_port = htons(client_list[i].port);

                    //printf("checking: first inet_aton\n");
                    if (inet_aton(client_list[i].ip, &serv_addr_ele.sin_addr)==0)
                    {
                        fprintf(stderr, "inet_aton() failed\n");
                        exit(1);
                    }
                    if (client_list[i].client_id > curr_ele_id)
                    {
                    	sprintf(temp, "%d", client_list[i].client_id);
                        strcpy(buf, "CLIENT_ID#");
                        strcat(buf, temp);
                    	send_msg(sockfd, buf, serv_addr_ele, slen);
                    }
                    //printf("checking: second inet_aton\n");
                }
                int received_ok = 0;

                while(1)
                {
                    if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&serv_addr, &slen) < 0)
                    {
                        if (received_ok == 0)
                        {
                            printf("I AM LEADER\n"); //BROADCAST TO ALL CLIENTS AND ELECETIONS
                            election = 1;
                            for (i = 0; i < total_clients; i++)
                            {
                                serv_addr_client.sin_port = htons(client_list[i].port);

                                if (inet_aton(client_list[i].ip, &serv_addr_client.sin_addr)==0)
                                {
                                    fprintf(stderr, "inet_aton() failed\n");
                                    exit(1);
                                }
                                //printf("checking: first inet_aton\n");
                                if (inet_aton(client_list[i].ip, &serv_addr_ele.sin_addr)==0)
                                {
                                    fprintf(stderr, "inet_aton() failed\n");
                                    exit(1);
                                }

                                sprintf(temp, "%d", curr_ele_id);
                                strcpy(buf, "I AM LEADER#");
                                strcat(buf, temp);
                                
                                send_msg(sockfd, buf, serv_addr_client, slen); //SENDING NEW LEADER TO CLIENT

                                if (client_list[i].client_id != curr_ele_id)
                                {
                                    
                                    send_msg(sockfd, buf, serv_addr_ele, slen); //SENDING NEW LEADER TO ELECTIONS
                                //printf("checking: second inet_aton\n");
                                }
                            }
                            break;
                        }
                        continue;
                    }
                    
                    detokenize(buf, token_result, "#");
                    //printf("%s\n", token_result[0]);
                    if (strcmp(token_result[0], "OK") == 0)
                    {
                        if(received_ok == 0)
                        {
                            received_ok = 1;
                        }
                    }
                    
                    if (strcmp(token_result[0], "CLIENT_ID") == 0)
                    {
                        send_msg(sockfd, "OK", serv_addr, slen);
                    }

                    if (strcmp (token_result[0], "I AM LEADER") == 0)
                    {
                        printf("New Leader: %s\n", token_result[1]);
                        election = 1;
                        break;
                    }
                    

                }
                	
                
            }
                        
            
        }
        
        if (election == 0)
        {

            detokenize(buf, token_result, "#");


            // if (strcmp(token_result[0], "ELECTION") == 0)
            // {
            // 	receive_msg(sockfd, buf, &serv_addr, &slen);
            // 	char* detokenized[BUFLEN]; 
            // 	detokenize(buf, token_result, "#");
            if (strcmp(token_result[0], "CLIENT_ID") == 0)
            {

                printf("here??\n");
            	send_msg(sockfd, "OK", serv_addr, slen);
                goto begin_election;
            }
            	//send_msg(sockfd, "OK", serv_addr, slen);

            //}
            //printf("reached here as well\n");
            
            if (strcmp(token_result[2], "STATUS") == 0)
            {
                send_msg(sockfd, "I AM ALIVE", serv_addr_seq, slen);
            }

            if (strcmp(token_result[0], "I AM LEADER") == 0)
            {
                printf("New Leader: %s\n", token_result[1]);
                election = 1;
            }
            
            //printf("First print\n");
            printf("%s\n", buf);
        }
        
    }
}  

// ele_port = PORT
// ele_addr = cli_addr
// ele_id clie_id
//CLIENT NEEDS TO KEEP WAITING AS LONG AS MSG IS ELECTION
