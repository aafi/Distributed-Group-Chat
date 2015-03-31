#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <sys/ioctl.h>

#define PORT 1705
#define MAXSIZE 1024
#define DELIMITER "#"

struct Leader{
	char ip_addr[MAXSIZE];
	char port[MAXSIZE];
}leader;

int client_id = 0;

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

void request_to_join(int soc, const char* my_ip_addr){
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

    if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("Could not send message to Sequencer\n");
		exit(-1);
	}

	if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, &serv_addr_size) < 0){
		perror("Error: Receiving message failed \n");
	} 

	char* join_details[MAXSIZE];
	detokenize(recvBuff, join_details, DELIMITER);

	if (strcmp(join_details[0], "SUCCESS") == 0){
		client_id = atoi(join_details[1]);
<<<<<<< HEAD
		printf("Entered success loop");
=======
		printf("%d\n", client_id);
>>>>>>> origin/dev
		if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, &serv_addr_size) < 0){
			perror("Error: Receiving message failed \n");
		} else {
			printf("%s\n", recvBuff);
		}
	} else {
		printf("Failed to join chat\n");
		exit(-1);
	}
}

int main(int argc, char* argv[]){
	int soc = 0, serv_addr_size;
	struct sockaddr_in my_addr;
	char* mode;
	char sendBuff[MAXSIZE], recvBuff[MAXSIZE];

	void* housekeeping(int);
	void* messenger(int);

	if(argc < 2){	//INVALID ARGUMENTS TO THE PROGRAM
		fprintf(stderr, "Invalid arguments. \nFormat: dchat USERNAME [IP-ADDR:PORT]\n");
		exit(-1);
	} else {		//RESPONSIBLE FOR CHECKING WHETHER THE CLIENT IS STARTING A NEW CHAT OR JOINING AN EXISTING ONE

		// CLIENT HAS TO BIND TO A LOCAL SOCKET IN ORDER TO COMMUNICATE - ALMOST ACTS LIKE A SERVER
		if ((soc = socket(AF_INET, SOCK_DGRAM, 0)) < 0){
			perror("ERROR: Could not create socket");
			exit(-1);
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

			// SET LEADER INFO TO ITS OWN IP_ADDRESS AND PORT NUMBER
			strcpy(leader.ip_addr, my_ip_addr);
			char temp[MAXSIZE];
		    // sprintf(temp, "%d", PORT);
		    sprintf(temp, "%d", 5678);
			strcpy(leader.port, temp);

			mode = "WAITING";
			fprintf(stderr, "%s started a new chat on %s:%d\n", argv[1], my_ip_addr, PORT);
			printf("Waiting for others to join:\n");

			request_to_join(soc, my_ip_addr);
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
			if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, &serv_addr_size) < 0){
				perror("ERROR: Receiving message failed \n");
			}

			char* leader_details[MAXSIZE];
			detokenize(recvBuff, leader_details, DELIMITER);

			// SET THE LEADER INFO TO WHAT IS RECEIVED FROM THE EXISTING CHAT CLIENT
			strcpy(leader.ip_addr, leader_details[1]);
			strcpy(leader.port, leader_details[2]);

			request_to_join(soc, my_ip_addr);
			/*
			TODO:
			- Update client list structure
			*/
		}
	}

	pthread_t threads[2];
	int rc0, rc1;
	rc0 = pthread_create(&threads[0], NULL, messenger, soc);
	rc1 = pthread_create(&threads[1], NULL, housekeeping, soc);
	pthread_join(threads[0], NULL);
	pthread_join(threads[1], NULL);
	pthread_exit(NULL);

	return 0;
}

void* housekeeping(int soc){
	char sendBuff[MAXSIZE], recvBuff[MAXSIZE];

	struct sockaddr_in other_user_addr;
	int other_addr_size;

	while(1){	// PUT THE SWITCH CASE FOR TYPES OF MESSAGES HERE TO PERFORM THAT PARTICULAT OPERATION!
		
		if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, &other_addr_size) < 0){
			perror("Error: Receiving message failed \n");
		} else {
			fprintf(stderr, "%s\n", recvBuff);
		}

		char* message[MAXSIZE];
		detokenize(recvBuff, message, DELIMITER);

		char messageType[MAXSIZE];
		strcpy(messageType, message[0]);

		if(strcmp(messageType, "JOIN") == 0){
			strcpy(sendBuff, "JOINLEADER#");
			strcat(sendBuff, leader.ip_addr);
			strcat(sendBuff, DELIMITER);
			strcat(sendBuff, leader.port);

			if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, sizeof(other_user_addr)) < 0){
				perror("ERROR: Sending message failed \n");
			}
		}
	} // end of while(1)
}

void* messenger(int soc){
	int msg_id = 0;
	char message[MAXSIZE];
	char user_input[MAXSIZE];
	while(1){
		printf("ME: ");
		memset(user_input, 0, MAXSIZE);
		fgets(user_input, MAXSIZE, stdin);

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

		// INITIATE COMMUNICATION WITH THE LEADER
		struct sockaddr_in serv_addr;

		serv_addr.sin_family = AF_INET;
		serv_addr.sin_port = htons(atoi(leader.port));
		if(inet_pton(AF_INET, leader.ip_addr, &serv_addr.sin_addr)<=0)
	    {
	        perror("ERROR: inet_pton error occured \n");
	        // exit(-1);
	    }
	    if (sendto(soc, message, MAXSIZE, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
			perror("Could not send message to Sequencer\n");
			// exit(-1);
		}
	} // end of while(1)
}