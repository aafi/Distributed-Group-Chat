#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <sys/ioctl.h>
#include <sys/time.h>
#include <signal.h>

#define PORT 1705
#define MAXSIZE 1024
#define DELIMITER "#"
#define MAX 15
#define ELE_PORT 8174
#define TIMEOUT_SEC 2
#define TIMEOUT_USEC 0
#define BUFLEN 1024
#define PORT_PING 5679

struct Leader{
	char ip_addr[MAXSIZE];
	char port[MAXSIZE];
}leader;

struct client{
   char ip[MAXSIZE];
   int port;
   int client_id;
   int last_msg_id;
   char name[MAXSIZE];
}client_list[MAX];

/*
Node structure for TAIL QUEUE
*/
struct node{
	int msg_id;
	int global_id;
	char message[MAXSIZE];
	int client_id;
	int acknowledged;
	TAILQ_ENTRY(node) entries;
};

TAILQ_HEAD(, node) queue_head; // head of the message queue
TAILQ_HEAD(, node) holdback_queue_head; // head of the holdback queue

int total_clients = 0;
int client_id = 0;

int last_global_seq_id = -1;	// The sequence id of the last message received from the sequencer/leader

int msg_id = 0;		// message id counter
int last_msg_id = 0;	// message counter needs to be frozen once election begins

int election = 0;	// Flag for if election is being held
int isLeader = 0;	// Flag for if the current client is the leader

int prog_exit = 0;	// Flag to cause threads to exit
int childId = -1;

pthread_mutex_t client_list_lock;
pthread_mutex_t holdback_queue_lock;
pthread_mutex_t message_queue_lock;

/*
method: detokenize
@buf: char[] - string that needs to be detokenized
@token_result: char* [] - list of strings that the detokenize function will fill up
@token: char* - delimiter string

Description: This function takes a string (buf) and a token (token) and splits the string and places individual strings in token_result
*/
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

// BELOW CODE IS TO FIND THE IP-ADDRESS SO THAT IT CAN BE PRINTED WHEN CLIENT STARTS NEW CHAT
/*
method: get_ip_address
returns: const char* - IP address of the local machine

Description: This function is used to get the IP Address of the machine it is running on.
	It makes a system call, strips out all the other data from the result, and saves the IP address in a string
*/
const char* get_ip_address(){
	FILE *fp;
	int status;
	char shell_output[MAXSIZE];

	fp = popen("/sbin/ifconfig | grep inet | head -n 1", "r");
	if (fp == NULL)
	    perror("Could not get IP address");

	fgets(shell_output, MAXSIZE, fp);

    status = pclose(fp);
	if (status == -1) {
	    perror("Error closing fp");
	}

	char* shell_result[MAXSIZE];
    detokenize(shell_output, shell_result, " ");

	char* addr_info[MAXSIZE];
	detokenize(shell_result[1], addr_info, ":");

	return addr_info[1];
}

/*
method: update_client_list
@all_client_details: char* [] - takes the detokenized message received from the leader containing the list of client info

This method is responsible for updating the client_list array with the new information received from the sequencer.
*/
void update_client_list(char* all_client_details[]){
	int i = 0;
	int j = 0;
	pthread_mutex_lock(&client_list_lock);
	memset(client_list, 0, MAX);
	total_clients = atoi(all_client_details[3]);
	int end_details = total_clients * 5 + 4;
	for (i = 4; i < end_details; i += 5){
		struct client clnt;

		strcpy(clnt.ip, all_client_details[i]);
		clnt.port = atoi(all_client_details[i + 1]);
		clnt.client_id = atoi(all_client_details[i + 2]);
		strcpy(clnt.name, all_client_details[i + 3]);
		clnt.last_msg_id = atoi(all_client_details[i + 4]);
		// clnt.last_msg_id = -1;
		client_list[j++] = clnt;
	}
	pthread_mutex_unlock(&client_list_lock);
}

/*
method: request_to_join
@soc: int - the socket that the current client is listening to
@my_ip_addr: const char* - string containing the ip_address of the current client

This method is responsible for handling the initial joining of a new client to the chat.
The client first sends the following information to the leader: REQUEST#my_ip_address#my_port_no
The leader replies with either a SUCCESS or FAILURE message

On FAILURE, the client exits
The SUCCESS message is formatted as: SUCCESS#client_id where the client_id is assigned to the new client by the leader
The client now waits for a multicast message from the leader, this message is used to update the list of existing clients in the system.
*/
void request_to_join(int soc, const char* my_ip_addr, char client_name[]){
	char sendBuff[MAXSIZE], recvBuff[MAXSIZE];
	// INITIATE COMMUNICATION WITH THE LEADER
	struct sockaddr_in serv_addr;
	int serv_addr_size;
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(leader.port));
	if(inet_pton(AF_INET, leader.ip_addr, &serv_addr.sin_addr)<=0)
    {
        perror("ERROR: inet_pton error occured \n");
        exit(-1);
    }

    strcpy(sendBuff, "REQUEST#");
    strcat(sendBuff, my_ip_addr);
    strcat(sendBuff, DELIMITER);
    char temp[MAXSIZE];
    sprintf(temp, "%d", PORT);
    strcat(sendBuff, temp);
    strcat(sendBuff, DELIMITER);
    strcat(sendBuff, client_name);

    if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Could not send message to Sequencer\n");
		exit(-1);
	}

	if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, &serv_addr_size) < 0){
		perror("Error: Receiving message failed \n");
	} else{
		// printf("AFTER REQUEST:%s\n", recvBuff);
	}

	char* join_details[MAXSIZE];
	detokenize(recvBuff, join_details, DELIMITER);

	if (strcmp(join_details[0], "SUCCESS") == 0){
		client_id = atoi(join_details[1]);
		last_global_seq_id = atoi(join_details[2]);
		// printf("UPDATED GLOBALSEQID TO: %d\n", last_global_seq_id);
		if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, &serv_addr_size) < 0){
			perror("Error: Receiving message failed \n");
		} else {
			// printf("%s\n", recvBuff);
			char* all_client_details[MAXSIZE];
			detokenize(recvBuff, all_client_details, DELIMITER);

			update_client_list(all_client_details);
		}
	} else {
		printf("Failed to join chat\n");
		exit(-1);
	}
}

void start_sequencer(soc){
	if ((childId = fork()) == 0){	// INSIDE SEQUENCER CHILD
		execv("sequencer", NULL);
	}
	// SET LEADER INFO TO ITS OWN IP_ADDRESS AND PORT NUMBER
	isLeader = 1;

	char recvBuff[MAXSIZE];
	struct sockaddr_in serv_addr;
	int serv_addr_size = sizeof(serv_addr);
	if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, &serv_addr_size) < 0){
		perror("ERROR: Receiving message failed \n");
	} 

	char* seq_info[MAXSIZE];
	detokenize(recvBuff, seq_info, DELIMITER);
	strcpy(leader.ip_addr, seq_info[2]);
	strcpy(leader.port, seq_info[3]);
}

int main(int argc, char* argv[]){
	int soc = 0, serv_addr_size;
	struct sockaddr_in my_addr;
	char* mode;
	char sendBuff[MAXSIZE], recvBuff[MAXSIZE];

	TAILQ_INIT(&queue_head);		// Initialize TAILQ
	TAILQ_INIT(&holdback_queue_head);		//Initialize TAILQ

	void* housekeeping(int);
	void* messenger(int);
	void* election_algorithm(int);
	void* message_display(int);



	if(argc < 2){	//INVALID ARGUMENTS TO THE PROGRAM
		fprintf(stderr, "Invalid arguments. \nFormat: dchat USERNAME [IP-ADDR PORT]\n");
		exit(-1);
	} else {		//RESPONSIBLE FOR CHECKING WHETHER THE CLIENT IS STARTING A NEW CHAT OR JOINING AN EXISTING ONE

		// CLIENT HAS TO BIND TO A LOCAL SOCKET IN ORDER TO COMMUNICATE - ALMOST ACTS LIKE A SERVER
		if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
			perror("ERROR: Could not create socket");
			exit(-1);
		}

		int reuse = 1;
		if (setsockopt(soc, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) 
	    {
	        perror("Setting socket timeout error");
	    }

		my_addr.sin_family = AF_INET;
		my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
		my_addr.sin_port = htons(PORT);

		if(bind(soc, (struct sockaddr*)&my_addr, sizeof(my_addr))){
			perror("ERROR: Could not bind");
			close(soc);
			exit(-1);
		}

		const char* my_ip_addr = get_ip_address();

		if(argc == 2){	
			/*
			START a new chat
			argv[1] = client name

			Responsible for starting the leader (sequencer) program
			Also needs to start Election Algorithm program
			*/

			start_sequencer(soc);

			my_ip_addr = get_ip_address();

			char msg[] = "NEWCHAT";
			struct sockaddr_in seq_addr;
			int seq_addr_size;
			seq_addr.sin_family = AF_INET;
			seq_addr.sin_port = htons(atoi(leader.port));
			if(inet_pton(AF_INET, leader.ip_addr, &seq_addr.sin_addr)<=0)
		    {
		        perror("ERROR: inet_pton error occured \n");
		        exit(-1);
		    }
		    if (sendto(soc, msg, MAXSIZE, 0, (struct sockaddr*)&seq_addr, sizeof(seq_addr)) < 0){
				fprintf(stderr, "ERROR: Sorry no chat is active on %s, try again later. \n", argv[2]);
				exit(-1);
			}

			mode = "WAITING";
			fprintf(stderr, "%s started a new chat, listening on %s:%d\n", argv[1], my_ip_addr, PORT);
			fprintf(stderr, "Succeeded, current users: \n%s %s:%d (LEADER)\n",argv[1],my_ip_addr,PORT);
			printf("Waiting for others to join....\n");

			request_to_join(soc, my_ip_addr, argv[1]);
		} else if(argc == 4){	
			/*
			JOIN an existing chat conversation
			argv[1] = client name
			argv[2] = IP-ADDRESS of the client already in the chat
			argv[3] = port number of the client already in the chat

			Must also start Election Algorithm program

			Needs to contact the leader (sequencer) to actually join the damn chat!
			*/
			mode = "JOINING";

			printf("%s joining a new chat on %s:%s, listening on %s:%d\n",argv[1],argv[2],argv[3],my_ip_addr,PORT);

			struct sockaddr_in serv_addr;

			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(atoi(argv[3]));

			if(inet_pton(AF_INET, argv[2], &serv_addr.sin_addr)<=0)
		    {
		        perror("ERROR: inet_pton error occured \n");
		        exit(-1);
		    }

		    strcpy(sendBuff, "JOIN");

		    if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
				fprintf(stderr, "ERROR: Sorry no chat is active on %s, try again later. \n", argv[2]);
				exit(-1);
			}

			struct timeval tv;
			tv.tv_sec = TIMEOUT_SEC;
		    tv.tv_usec = TIMEOUT_USEC;

		    if (setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
		    {
		        perror("Setting socket timeout error");
		    }

			if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, &serv_addr_size) < 0){
				printf("Sorry, no chat is active on %s:%s, try again later. \nBye.\n", argv[2], argv[3]);
				exit(-1);
			} 

			// reset timeout so that socket behaves normally
			tv.tv_sec = 0;
		    tv.tv_usec = 0;

		    if (setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
		    {
		        perror("Setting socket timeout error");
		    }

			char* leader_details[MAXSIZE];
			detokenize(recvBuff, leader_details, DELIMITER);

			// SET THE LEADER INFO TO WHAT IS RECEIVED FROM THE EXISTING CHAT CLIENT
			strcpy(leader.ip_addr, leader_details[1]);
			strcpy(leader.port, leader_details[2]);

			request_to_join(soc, my_ip_addr, argv[1]);
			printf("%s joined the chat\n", argv[1]);
		}
	}

	pthread_t threads[3], ea_thread;
	int rc0, rc1, rc2, ea_status;
	rc0 = pthread_create(&threads[0], NULL, messenger, soc);
	rc1 = pthread_create(&threads[1], NULL, housekeeping, soc);
	rc2 = pthread_create(&threads[2], NULL, message_display, soc);
	ea_status = pthread_create(&ea_thread, NULL, election_algorithm, client_id);
	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);
	pthread_join(threads[2], NULL);
	pthread_join(ea_thread, NULL);
	// 	printf("Success joining EA thread!\n");
	// printf("Main about to exit-%d\n", prog_exit);
	close(soc);
	if (childId != -1)
		kill(childId, SIGKILL);	

	return 0;
}

void* housekeeping(int soc){
	char sendBuff[MAXSIZE], recvBuff[MAXSIZE];

	struct sockaddr_in other_user_addr;
	int other_addr_size = sizeof(other_user_addr);

	struct timeval tv;
	tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = TIMEOUT_USEC;

    if (setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
    {
        perror("Setting socket timeout error");
    }

	while(prog_exit == 0){	// PUT THE SWITCH CASE FOR TYPES OF MESSAGES HERE TO PERFORM THAT PARTICULAT OPERATION!
		
		if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, &other_addr_size) < 0){
			// If I don't receive a message, loop out and check if the program should still be running
		} else {
			fprintf(stderr, "CLIENT RECEIVED: %s\n", recvBuff);

			char* message[MAXSIZE], copy[MAXSIZE];
			strcpy(copy, recvBuff);
			detokenize(recvBuff, message, DELIMITER);

			char messageType[MAXSIZE];
			strcpy(messageType, message[0]);

			if(strcmp(messageType, "JOIN") == 0){
				strcpy(sendBuff, "JOINLEADER#");
				strcat(sendBuff, leader.ip_addr);
				strcat(sendBuff, DELIMITER);
				strcat(sendBuff, leader.port);

				if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, sizeof(other_user_addr)) < 0){
					perror("ERROR: Sending message failed for JOINLEADER \n");
				}
			} else if(strcmp(messageType, "MSG") == 0){
				//Handle displaying of message
				// printf("%s\n", copy);
				char client_name[MAXSIZE];
				int clientId = atoi(message[2]);
				//find the client name:
				int i = 0;
				for(i = 0; i < total_clients; ++i){
					if (clientId == client_list[i].client_id){
						strcpy(client_name, client_list[i].name);
						client_list[i].last_msg_id = atoi(message[3]);
						break;
					}
				}
				int globalSeqNo = atoi(message[1]);

				// ADD MESSAGE TO HOLDBACK_QUEUE
				struct node *item, *temp_item;
				item = malloc(sizeof(*item));
				item->msg_id = atoi(message[3]);
				item->acknowledged = 0;
				item->global_id = globalSeqNo;
				item->client_id = clientId;
				strcpy(item->message, message[4]);

				pthread_mutex_lock(&holdback_queue_lock);
				TAILQ_INSERT_TAIL(&holdback_queue_head, item, entries);
				// pthread_mutex_unlock(&holdback_queue_lock);

				// check if next message is present in queue or not, else send a request for it LOST#global_seq_id
				int found = 0;
				// pthread_mutex_lock(&holdback_queue_lock);
				for(item = TAILQ_FIRST(&holdback_queue_head); item != NULL; item = TAILQ_NEXT(item, entries)){
					// temp_item = TAILQ_NEXT(item, entries);

					if(item->global_id == last_global_seq_id){
						found = 1;
						break;
					}
				}
				pthread_mutex_unlock(&holdback_queue_lock);
				if(found == 0){
					strcpy(sendBuff, "LOST#");
					char temp[MAXSIZE];
					sprintf(temp, "%d", last_global_seq_id);
					strcat(sendBuff, temp);

					printf("LOST MESSAGE: %s\n", sendBuff);
					if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, sizeof(other_user_addr)) < 0){
						perror("ERROR: Sending message failed in ACK \n");
					} 
				}
			} else if(strcmp(messageType, "ELECTION") == 0){	// Election is taking place
				if (isLeader == 1){
					strcpy(sendBuff, "CANCEL");
					if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, sizeof(other_user_addr)) < 0){
						perror("ERROR: Sending message failed in ACK \n");
					} 
				} else{
					election = 1;
				}
			} else if(strcmp(messageType, "ELECTIONCANCEL") == 0){	// Election has been cancelled
				election = 0;
			} else if(strcmp(messageType, "LEADER") == 0){	// Client is the new leader
				printf("%s\n", recvBuff);
				isLeader = 1;
				char old_leader_ip[MAXSIZE];
				strcpy(old_leader_ip, leader.ip_addr);
				start_sequencer(soc);

				char buf_part1[MAXSIZE], buf_part2[MAXSIZE], buf_part3[MAXSIZE];
				strcpy(buf_part3, "");

				strcpy(buf_part1, "NEWLEADER#");
				char temp[MAXSIZE];
				sprintf(temp, "%d", total_clients - 1);
				strcat(buf_part1, temp);
				strcat(buf_part1, DELIMITER);

				int count = 0;
				for (count = 0; count < total_clients; ++count){
					if (strcmp(old_leader_ip, client_list[count].ip) != 0){
						strcat(buf_part3, client_list[count].ip);
						strcat(buf_part3, DELIMITER);
						char temp[MAXSIZE];
						sprintf(temp, "%d", client_list[count].port);
						strcat(buf_part3, temp);
						strcat(buf_part3, DELIMITER);
						sprintf(temp, "%d", client_list[count].client_id);
						strcat(buf_part3, temp);
						strcat(buf_part3, DELIMITER);
						strcat(buf_part3, client_list[count].name);
						strcat(buf_part3, DELIMITER);
						sprintf(temp, "%d", client_list[count].last_msg_id);
						strcat(buf_part3, temp);
						strcat(buf_part3, DELIMITER);
					}
					else
					{
						strcpy(buf_part2, "NOTICE ");
						strcat(buf_part2, client_list[count].name);
						strcat(buf_part2, " left the chat or crashed#");
					}
				}
				sprintf(sendBuff, "%s%s%s", buf_part1, buf_part2, buf_part3);

				other_user_addr.sin_family = AF_INET;
				other_user_addr.sin_port = htons(atoi(leader.port));
				if(inet_pton(AF_INET, leader.ip_addr, &other_user_addr.sin_addr)<=0)
			    {
			        perror("ERROR: inet_pton error occured \n");
			        exit(-1);
			    }
				if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, sizeof(other_user_addr)) < 0){
					perror("ERROR: Sending message failed \n");
				}
				election = 0;
			} else if(strcmp(messageType, "SEQ") == 0){		// HANDLES ALL LEADER RELATED MESSAGES!
				char seq_message_type[MAXSIZE];
				strcpy(seq_message_type, message[1]);

				if (strcmp(seq_message_type, "CLIENT") == 0){
					update_client_list(message);
				} else if (strcmp(seq_message_type, "ACK") == 0){
					// Acknowledgement received 
					// IF ACKNOWLEDGEMENTS ARE NOT RECEIVED, AFTER A TIMEOUT, RESEND THE MESSAGE

					// THIS MAY ACTUALLY NOT GO HERE, MIGHT NEED TO GO IN THE MESSENGER FUNCTION! NEED TO FIGURE THIS OUT
					int message_id = atoi(message[2]);
					struct node *item, *temp_item;
					pthread_mutex_lock(&message_queue_lock);
					for(item = TAILQ_FIRST(&queue_head); item != NULL; item = TAILQ_NEXT(item, entries)){
						// temp_item = TAILQ_NEXT(item, entries);

						if (item->msg_id == message_id){
							item->acknowledged = 1;
						}
					}
					pthread_mutex_unlock(&message_queue_lock);
				} else if (strcmp(seq_message_type, "REM") == 0){
					// Remove message from queue because sequencer has acknowledged receipt from all clients
					struct node *item, *temp_item;
					int message_id = atoi(message[2]);
					pthread_mutex_lock(&message_queue_lock);
					for(item = TAILQ_FIRST(&queue_head); item != NULL; item = temp_item){
						temp_item = TAILQ_NEXT(item, entries);

						if(item->msg_id == message_id){
							// printf("Removing message from MQ: %d-%s\n", item->msg_id, item->message);
							TAILQ_REMOVE(&queue_head, item, entries);
							free(item);
							break;
						}
					}
					pthread_mutex_unlock(&message_queue_lock);
				} else if (strcmp(seq_message_type, "REMHB") == 0){
					// Remove message form holdback queue based on broadcast from sequencer
					struct node *item, *temp_item;
					int seq_id = atoi(message[2]);
					pthread_mutex_lock(&holdback_queue_lock);
					for(item = TAILQ_FIRST(&holdback_queue_head); item != NULL; item = temp_item){
						temp_item = TAILQ_NEXT(item, entries);

						if(item->global_id == seq_id){
							// printf("Removing message from HB: %d-%s\n", item->msg_id, item->message);
							TAILQ_REMOVE(&holdback_queue_head, item, entries);
							free(item);
							break;
						}
					}
					pthread_mutex_unlock(&holdback_queue_lock);
				} else if (strcmp(seq_message_type, "STATUS") == 0){
					printf("%s\n", message[2]);
				} else if (strcmp(seq_message_type, "EXIT") == 0){
					printf("%s\n", message[3]);
					int clientId = atoi(message[2]);
					if (client_id == clientId){
						prog_exit = 1;
						if (isLeader == 1){
							kill(childId, SIGKILL);	
						}
					}
				} else if (strcmp(seq_message_type, "EA") == 0){
					strcpy(leader.ip_addr, message[2]);
					strcpy(leader.port, message[3]);
				} else if (strcmp(seq_message_type, "HB") == 0){
					strcpy(sendBuff, "HB#");
					char temp[MAXSIZE];
					sprintf(temp, "%d", client_id);
					strcat(sendBuff, temp);
					strcat(sendBuff, DELIMITER);

					sprintf(temp, "%d", last_global_seq_id);
					strcat(sendBuff, temp);
					strcat(sendBuff, DELIMITER);

					int hb_count = 0;
					struct node *item, *temp_item;
					// TAILQ_FOREACH(item, &holdback_queue_head, entries) {
					pthread_mutex_lock(&holdback_queue_lock);
					for(item = TAILQ_FIRST(&holdback_queue_head); item != NULL; item = TAILQ_NEXT(item, entries)){
						// temp_item = TAILQ_NEXT(item, entries);
		                ++hb_count;
			        }
			        pthread_mutex_unlock(&holdback_queue_lock);

			        sprintf(temp, "%d", hb_count);
			        strcat(sendBuff, temp);
			        strcat(sendBuff, DELIMITER);

			        // TAILQ_FOREACH(item, &holdback_queue_head, entries) {
			        pthread_mutex_lock(&holdback_queue_lock);
			        for(item = TAILQ_FIRST(&holdback_queue_head); item != NULL; item = TAILQ_NEXT(item, entries)){
						// temp_item = TAILQ_NEXT(item, entries);

		                sprintf(temp, "%d", item->global_id);
		                strcat(sendBuff, temp);
		                strcat(sendBuff, DELIMITER);

		                sprintf(temp, "%d", item->client_id);
		                strcat(sendBuff, temp);
		                strcat(sendBuff, DELIMITER);

		                sprintf(temp, "%d", item->msg_id);
		                strcat(sendBuff, temp);
		                strcat(sendBuff, DELIMITER);

		                strcat(sendBuff, item->message);
		                strcat(sendBuff, DELIMITER);
			        }
			        pthread_mutex_unlock(&holdback_queue_lock);

			        if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, sizeof(other_user_addr)) < 0){
						perror("ERROR: Sending message failed \n");
					}
				} else if (strcmp(seq_message_type, "SENDALL") == 0){
					election = 0;
					struct node *item, *temp_item;
					printf("INSIDE SENDALL\n");
					// TAILQ_FOREACH(item, &queue_head, entries){
					pthread_mutex_lock(&message_queue_lock);
					for(item = TAILQ_FIRST(&queue_head); item != NULL; item = TAILQ_NEXT(item, entries)){
						// temp_item = TAILQ_NEXT(item, entries);

						strcpy(sendBuff, "MESSAGE#");
						char clientId[MAXSIZE];
						sprintf(clientId, "%d", item->client_id);
						strcat(sendBuff, clientId);
						strcat(sendBuff, DELIMITER);
						char messageId[MAXSIZE];
						sprintf(messageId, "%d", item->msg_id);
						strcat(sendBuff, messageId);
						strcat(sendBuff, DELIMITER);
						strcat(sendBuff, item->message);

						printf("SENDALL: %d-%s\n", item->msg_id, item->message);
						
						if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, sizeof(other_user_addr)) < 0){
							perror("ERROR: Sending message failed \n");
						}
					}
					pthread_mutex_unlock(&message_queue_lock);
				}
			} // end of leader related else-if
		}
	} // end of while(1)
}

void* messenger(int soc){
	msg_id = 0;		// The message id of the last message sent by this client to the sequencer/leader
	char message[MAXSIZE];
	char user_input[MAXSIZE];
	// char eof_str[MAXSIZE];
	// sprintf(eof_str, "%d", EOF);
	while(prog_exit == 0){
		memset(user_input, 0, MAXSIZE);
		// fgets(user_input, MAXSIZE, stdin);

		if (fgets(user_input, MAXSIZE, stdin) == NULL){
			// exit all threads
			prog_exit = 1;
		} else {
			strcpy(message, "MESSAGE#");
			char clientId[MAXSIZE];
			sprintf(clientId, "%d", client_id);
			strcat(message, clientId);
			strcat(message, DELIMITER);
			char messageId[MAXSIZE];
			sprintf(messageId, "%d", msg_id++);
			strcat(message, messageId);
			strcat(message, DELIMITER);
			strcat(message, user_input);

			// ADD MESSAGE TO CLIENT_QUEUE
			struct node *item;
			item = malloc(sizeof(*item));
			item->msg_id = msg_id - 1;
			item->acknowledged = 0;
			item->client_id = client_id;
			strcpy(item->message, user_input);

			pthread_mutex_lock(&message_queue_lock);
			printf("INSERTING INTO QUEUE: %d-%s\n", item->msg_id, item->message);
			TAILQ_INSERT_TAIL(&queue_head, item, entries);
			pthread_mutex_unlock(&message_queue_lock);

			// INITIATE COMMUNICATION WITH THE LEADER
			if(election == 0){
				struct sockaddr_in serv_addr;

				serv_addr.sin_family = AF_INET;
				serv_addr.sin_port = htons(atoi(leader.port));
				if(inet_pton(AF_INET, leader.ip_addr, &serv_addr.sin_addr)<=0)
			    {
			        perror("ERROR: inet_pton error occured \n");
			        // exit(-1);
			    }
			    printf("CLIENT SENDING: %s\n", message);
			    if (sendto(soc, message, MAXSIZE, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
					perror("Could not send message to Sequencer\n");
					// exit(-1);
				}
			}	// end of election check if
		}
	} // end of while(1)
}

void* message_display(soc){
	while(prog_exit == 0){
		int i = 0;
		char client_name[MAXSIZE], sendBuff[MAXSIZE];

		struct sockaddr_in serv_addr;

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(atoi(leader.port));
		if(inet_pton(AF_INET, leader.ip_addr, &serv_addr.sin_addr)<=0)
	    {
	        perror("ERROR: inet_pton error occured \n");
	        // exit(-1);
	    }

		struct node *item, *temp_item;
		pthread_mutex_lock(&holdback_queue_lock);
		for(item = TAILQ_FIRST(&holdback_queue_head); item != NULL; item = TAILQ_NEXT(item, entries)){
			// temp_item = TAILQ_NEXT(item, entries);
			
            if(item->global_id == last_global_seq_id){
            	// need to find client name for printing
            	for(i = 0; i < total_clients; ++i){
            		if (item->client_id == client_list[i].client_id){
            			strcpy(client_name, client_list[i].name);
            		}
            	}

            	printf("%d-%s: %s", last_global_seq_id, client_name, item->message);
            	last_global_seq_id = item->global_id + 1;

            	// send back acknowledgement to sequencer: ACK#client_id#global_seq_id
				strcpy(sendBuff, "ACK");
				strcat(sendBuff, DELIMITER);
				char temp[MAXSIZE];
				sprintf(temp, "%d", client_id);
				strcat(sendBuff, temp);
				strcat(sendBuff, DELIMITER);
				sprintf(temp, "%d", item->global_id);
				strcat(sendBuff, temp);

				if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
					perror("ERROR: Sending message failed in ACK \n");
				} 
            	break;
            }
        }
        pthread_mutex_unlock(&holdback_queue_lock);
	}
}

/*
ELECTION ALGORITHM SPACE

All functions below this point belong to the election algorithm
*/
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

void* election_algorithm(int curr_id){
	struct sockaddr_in my_addr, serv_addr_seq, serv_addr_client, serv_addr_ele, serv_addr;
    int sockfd, i, election = 0;
    socklen_t slen=sizeof(serv_addr_seq);
    char buf[BUFLEN], temp[BUFLEN], curr_ele_id[BUFLEN], new_leader_ip[BUFLEN];
    char* token_result[BUFLEN];
    sprintf(curr_ele_id, "%d", curr_id);
    // if(argc != 4)
    // {
    //   printf("Usage : %s <Sequencer Server-IP> <sequencer port> <client_id> \n",argv[0]);
    //   exit(0);
    // }
    
    // populate_clients();
    //strcpy(buf, argv[3]);
    
    // curr_ele_id = atoi(argv[3]);

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0))==-1)
        err("socket");

    int reuse = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(int)) < 0) 
    {
        perror("Setting socket timeout error");
    }

    //calling_client_addr = argv[1]
    
    bzero(&my_addr, sizeof(my_addr));
    bzero(&serv_addr_seq, sizeof(serv_addr_seq));
    bzero(&serv_addr_client, sizeof(serv_addr_client));
    bzero(&serv_addr_ele, sizeof(serv_addr_ele));

    my_addr.sin_family = AF_INET;
    serv_addr_seq.sin_family = AF_INET;
    serv_addr_client.sin_family = AF_INET;
    serv_addr_ele.sin_family = AF_INET;

    serv_addr_seq.sin_port = htons(PORT_PING);
    my_addr.sin_port = htons(ELE_PORT);
    serv_addr_ele.sin_port = htons(ELE_PORT);

    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sockfd, (struct sockaddr* ) &my_addr, sizeof(my_addr))==-1)
      err("bind");

    if (inet_aton(leader.ip_addr, &serv_addr_seq.sin_addr)==0)
    {
        fprintf(stderr, "inet_aton() failed\n");
        exit(1);
    }

    //printf("In election algorithm\n");

    struct timeval tv;
    tv.tv_sec = TIMEOUT_SEC;
    tv.tv_usec = TIMEOUT_USEC;

    while(prog_exit == 0)
    {
    	// if (election == 1)
    		// printf("Out of election\n");
    	int won = 0;
  //   	if (inet_aton(leader.ip_addr, &serv_addr_seq.sin_addr)==0)
		// {
		//     fprintf(stderr, "inet_aton() failed\n");
		//     exit(1);
		// }
		//printf("IP Address of Seq: %s\n", leader.ip_addr);
    	election = 0;
    	nanosleep((struct timespec[]){{0, 100000000}}, NULL);
    	//sleep(1);
        if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
        {
            perror("Error");
        }
        
        strcpy(buf, "PING#");
        strcat(buf, curr_ele_id);
        send_msg(sockfd, buf, serv_addr_seq, slen); //PING SEQUENCER TO CHECK IF IT IS ACTIVE
        //printf("Pinged sequencer\n");
        if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&serv_addr, &slen) < 0)
        {
            //TIMEOUT REACHED -> SEQUENCER IS NOT ACTIVE
            //printf("timeout\n");
            strcpy(buf, "PING#");
            strcat(buf, curr_ele_id);
            send_msg(sockfd, buf, serv_addr_seq, slen);
            if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&serv_addr, &slen) < 0) //DOUBLE CHECKING SEQ CRASH
            {
                
                begin_election:
                election = 1;
                //printf("Starting election\n");
                for (i = 0; i < total_clients; i++) //INFORMING ALL CLIENTS THAT ELECTION IS BEING HELD
                {
                	serv_addr_client.sin_port = htons(client_list[i].port);

                    if (inet_aton(client_list[i].ip, &serv_addr_client.sin_addr)==0)
                    {
                        fprintf(stderr, "Client inet_aton() failed for sending election\n");
                        exit(1);
                    }
                    //printf("Inofrming: first inet_aton\n");

                    //printf("informing: 2nd inet_aton\n");

                    
                    send_msg(sockfd, "ELECTION", serv_addr_client, slen);
                    

                }

                for (i = 0; i < total_clients; i++) //SENDING ID TO ALL HIGHER ID ELECTIONS (MAKE THIS A FUNCTION??)
                {
                	//serv_addr_client.sin_port = htons(client_list[i].port);
                	//serv_addr_ele.sin_port = htons(client_list[i].port);

                    //printf("checking: first inet_aton\n");
                    if (inet_aton(client_list[i].ip, &serv_addr_ele.sin_addr)==0)
                    {
                        fprintf(stderr, "Election inet_aton() of higher ids failed\n");
                        exit(1);
                    }
                    if (client_list[i].client_id > atoi(curr_ele_id))
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
                	//printf("Waiting on receieve\n");
                    if (recvfrom(sockfd, buf, BUFLEN, 0, (struct sockaddr*)&serv_addr, &slen) < 0)
                    {

                        if ((received_ok == 0) && (won == 0))
                        {
                            //printf("I AM LEADER\n"); //BROADCAST TO ALL ELECTIONS AND WINNING CLIENT
                            election = 1;
                            won = 1;
                            //printf("Found new leader\n");
                            for (i = 0; i < total_clients; i++)
                            {
                            	//if (client_list[i].client_id != atoi(curr_ele_id))
                            	//{


	                                serv_addr_client.sin_port = htons(client_list[i].port);

	                                if (inet_aton(client_list[i].ip, &serv_addr_client.sin_addr)==0)
	                                {
	                                    fprintf(stderr, "Client inet_aton() for sending new leader failed\n");
	                                    exit(1);
	                                }
	                                //printf("checking: first inet_aton\n");
	                                if (inet_aton(client_list[i].ip, &serv_addr_ele.sin_addr)==0)
	                                {
	                                    fprintf(stderr, "Election inet_aton() for sending new leader failed\n");
	                                    exit(1);
	                                }

	                                // sprintf(temp, "%d", curr_ele_id);
	                                strcpy(buf, "I AM LEADER#");
	                                strcat(buf, curr_ele_id);
	                                //printf("SENDING END OF ELECTION\n");
	                                send_msg(sockfd, buf, serv_addr_ele, slen); //SENDING NEW LEADER TO ELECTIONS
                                //} 
	                                
                                if (client_list[i].client_id == atoi(curr_ele_id))
                                {
                                    
                                    send_msg(sockfd, "LEADER", serv_addr_client, slen); //SENDING NEW LEADER TO WINNING CLIENT
                                //printf("checking: second inet_aton\n");
                                }

                            }
                            
                            memset(buf, 0, BUFLEN);
                        }
                        continue;
                    }

                    //printf("message received by election: %s\n", buf);
                    
                    detokenize(buf, token_result, "#");
                    //printf("buf: %s\n", buf);
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
                        //printf("New Leader: %s\n", token_result[1]);
                        election = 1;
                        //printf("END OF ELECTION 1\n");
                        tv.tv_sec = 0; //RESETTING TIMEOUT TO 0
					    tv.tv_usec = 0;
					    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
				        {
				            perror("Error");
				        }
                        
                        receive_msg(sockfd, buf, &serv_addr, &slen); //WAITING FOR NEW SEQUENCER TO START
                        //printf("Received from sequencer: %s\n", buf);
                        
                        detokenize(buf, token_result, "#");
                        strcpy(new_leader_ip, token_result[2]);
                        
                        if (inet_aton(new_leader_ip, &serv_addr_seq.sin_addr)==0)
						{
						    fprintf(stderr, "New sequencer inet_aton() failed\n");
						    exit(1);
						}

                        tv.tv_sec = TIMEOUT_SEC; //RESETTING TIMEOUT BACK TO NORMAL
					    tv.tv_usec = TIMEOUT_USEC;
					    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
				        {
				            perror("Error");
				        }
                        break;
                    }

                    if (strcmp(buf, "CANCEL") == 0)
                    {
                    	for (i = 0; i < total_clients; i++)
                        {
                            serv_addr_client.sin_port = htons(client_list[i].port);

                            if (inet_aton(client_list[i].ip, &serv_addr_client.sin_addr)==0)
                            {
                                fprintf(stderr, "Client inet_aton() for election cancel failed\n");
                                exit(1);
                            }
                    	    send_msg(sockfd, "ELECTIONCANCEL", serv_addr_client, slen);
                    	}
                    	    //printf("ELECTION CANCELLED\n");
                    	    break;
                    }

           //          if (strcmp(token_result[0], "STATUS") == 0)
			        // {
			        // 	printf("ELECTION: I'm still here!\n");
			        //     send_msg(sockfd, "I AM ALIVE", serv_addr, slen);
			        // }
                    

                }
                	
                
            }
                        
            
        }

        if (election == 0)
        {
        	//printf("message received by election: %s\n", buf);

            detokenize(buf, token_result, "#");


            // if (strcmp(token_result[0], "ELECTION") == 0)
            // {
            // 	receive_msg(sockfd, buf, &serv_addr, &slen);
            // 	char* detokenized[BUFLEN]; 
            // 	detokenize(buf, token_result, "#");
            if (strcmp(token_result[0], "CLIENT_ID") == 0)
            {

                //printf("here??\n");
            	send_msg(sockfd, "OK", serv_addr, slen);
                goto begin_election;
            }
            	//send_msg(sockfd, "OK", serv_addr, slen);

            //}
            //printf("reached here as well\n");
            
            // if (strcmp(token_result[0], "STATUS") == 0)
            // {
            // 	printf("ELECTION: I'm still here!\n");
            //     send_msg(sockfd, "I AM ALIVE", serv_addr, slen);
            // }

            if (strcmp(token_result[0], "I AM LEADER") == 0)
            {
            	//receive_msg(sockfd, buf, &serv_addr, &slen);
                //printf("END OF ELECTION 2\n");
                
                tv.tv_sec = 0; //RESETTING TIMEOUT TO 0
			    tv.tv_usec = 0;
			    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
		        {
		            perror("Error");
		        }
                
                receive_msg(sockfd, buf, &serv_addr, &slen); //WAITING FOR NEW SEQUENCER TO START
                
                //printf("Received from sequencer (No crash detected): %s\n", buf);

                detokenize(buf, token_result, "#");
                strcpy(new_leader_ip, token_result[2]);
                
                if (inet_aton(new_leader_ip, &serv_addr_seq.sin_addr)==0)
				{
				    fprintf(stderr, "New sequencer inet_aton() failed\n");
				    exit(1);
				}

                tv.tv_sec = TIMEOUT_SEC; //RESETTING TIMEOUT BACK TO NORMAL
			    tv.tv_usec = TIMEOUT_USEC;
			    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO,&tv,sizeof(tv)) < 0) 
		        {
		            perror("Error");
		        }

                election = 1;
            }
            
        }
        //printf("BUF: %s\n", buf);
        
    }
    close(sockfd);
    // printf("Goodbye from the ELECTION ALGORITHM\n");
    // pthread_exit(NULL);
}