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
#define TIMEOUT_SEC 1
#define TIMEOUT_USEC 500000


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

int client_count = 2;
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

    strcpy(c.ip, "127.0.0.1");
    c.port = 4567;
	c.client_id = 1;

	client_list[0] = c;

	strcpy(c.ip, "127.0.0.1");
	c.port = 4568;
	c.client_id = 2;

	client_list[1] = c;

}


// void populate_clients(char* data[MAX])
// {
//     int i, j = 0;
//     for (i = 0; i < len(data) - 2; i++)
//     {
//     	switch(i % 3)
//     	{
//     		case 0: client_list[j].ip = data[i + 2];
//     		break;

//     		case 1: client_list[j].port = atoi(data[i + 2]);
//     		break;

//     		case 2: client_list[j++].client_id = atoi(data[i + 2]);
//     	}
//     }

//     client_count = j;
// }

// void populate_elections(char* data[MAX]) //UPDATE BY GETTING INFO FROM CLIENT STRUCT
// {
//     int i, j = 0;
//     for (i = 0; i < len(data) - 2; i++)
//     {
//     	switch(i % 3)
//     	{
//     		case 0: ele_list[j].ip = data[i + 2];
//     		break;

//     		case 1: ele_list[i].port = atoi(data[i + 2]);
//     		break;

//     		case 2: ele_list[j++].ele_id = atoi(data[i+2]);
//     	}
//     }
// }

int main(int argc, char** argv)
{
    struct sockaddr_in my_addr, serv_addr_seq, serv_addr_client, serv_addr_ele, serv_addr;
    int sockfd, i;
    socklen_t slen=sizeof(serv_addr_seq);
    char buf[BUFLEN], temp[BUFLEN];
    if(argc != 4)
    {
      printf("Usage : %s <Server-IP> <port> <client_id> \n",argv[0]);
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

    while(1)
    {
    	struct timeval tv;
        tv.tv_sec = TIMEOUT_SEC;
        tv.tv_usec = TIMEOUT_USEC;
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
        {
            perror("Error");
        }
        
        send_msg(sockfd, "There?", serv_addr_seq, slen); //PING SEQUENCER TO CHECK IF IT IS ACTIVE

        if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&serv_addr_seq, &slen) < 0)
        {
            //TIMEOUT REACHED -> SEQUENCER IS NOT ACTIVE
            printf("timeout\n");
            for (i = 0; i < client_count; i++)
            {
            	serv_addr_client.sin_port = htons(client_list[i].port);
            	serv_addr_ele.sin_port = htons(PORT);

                if (inet_aton(client_list[i].ip, &serv_addr_client.sin_addr)==0)
                {
                    fprintf(stderr, "inet_aton() failed\n");
                    exit(1);
                }
                if (inet_aton(client_list[i].ip, &serv_addr_ele.sin_addr)==0)
                {
                    fprintf(stderr, "inet_aton() failed\n");
                    exit(1);
                }

            	send_msg(sockfd, "ELECTION", serv_addr_client, slen); //TELL CLIENTS TO STOP FOR ELECTION AND WAIT FOR NEW LEADER
            	send_msg(sockfd, "ELECTION", serv_addr_ele, slen); //TELL ELECTIONS TO STOP PINGING AND PARTICIPATE IN ELECTION

            	if (client_list[i].client_id == curr_ele_id)
            	{
            		receive_msg(sockfd, buf, &serv_addr, &slen);
            	}
            }

            for (i = 0; i < client_count; i++) //MAKE THIS A FUNCTION
            {
            	serv_addr_client.sin_port = htons(client_list[i].port);
            	serv_addr_ele.sin_port = htons(client_list[i].port);

                if (inet_aton(client_list[i].ip, &serv_addr_client.sin_addr)==0)
                {
                    fprintf(stderr, "inet_aton() failed\n");
                    exit(1);
                }
                if (inet_aton(ele_list[i].ip, &serv_addr_ele.sin_addr)==0)
                {
                    fprintf(stderr, "inet_aton() failed\n");
                    exit(1);
                }
                if (client_list[i].client_id > curr_ele_id)
                {
                	sprintf(temp, "%d", client_list[i].client_id);
                	send_msg(sockfd, strcat("CLIENT_ID#", temp), serv_addr_ele, slen);
                	receive_msg (sockfd, buf, &serv_addr, &slen); //If timeout occurs broadcast "I am Leader"
                }

            	
            }
                        
            
        }

        if (strcmp(buf, "ELECTION") == 0)
        {
        	receive_msg(sockfd, buf, &serv_addr, &slen);
        	char* detokenized[BUFLEN]; 
        	detokenize(buf, detokenized, "#");
        	if (strcmp(detokenized[0], "CLIENT_ID") == 0)
        	{
        		send_msg(sockfd, "OK", serv_addr, slen);
        	}
        	send_msg(sockfd, "OK", serv_addr, slen);

        }

        printf("%s\n", buf);
        
    }
}  

// ele_port = PORT
// ele_addr = cli_addr
// ele_id clie_id
