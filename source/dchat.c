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

int main(int argc, char* argv[]){
	int soc = 0, serv_addr_size, other_addr_size;
	struct sockaddr_in my_addr, serv_addr;
	struct sockaddr_un other_user_addr;
	char* mode;
	char sendBuff[MAXSIZE], recvBuff[MAXSIZE];

	if(argc < 2){	//INVALID ARGUMENTS TO THE PROGRAM
		fprintf(stderr, "Invalid arguments. \nFormat: dchat USERNAME [IP-ADDR:PORT]\n");
		exit(-1);
	} else {		//RESPONSIBLE FOR CHECKING WHETHER THE CLIENT IS STARTING A NEW CHAT OR JOINING AN EXISTING ONE

		// CLIENT HAS TO BIND TO A LOCAL SOCKET IN ORDER TO COMMUNICATE - ALMOST ACTS LIKE A SERVER
		if ((soc = socket(PF_INET, SOCK_DGRAM, 0)) < 0){
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

		// BELOW CODE IS TO FIND THE IP-ADDRESS SO THAT IT CAN BE PRINTED WHEN CLIENT STARTS NEW CHAT
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

		char* ip_addr = addr_info[1];

		if(argc == 2){	
			/*
			START a new chat
			argv[1] = client name

			Responsible for starting the leader (sequencer) program
			Also needs to start Election Algorithm program
			*/
			mode = "WAITING";
			fprintf(stderr, "%s started a new chat on %s:%d\n", argv[1], ip_addr, PORT);
			printf("Waiting for others to join:\n");
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
			} else {
				fprintf(stderr, "%s \n", recvBuff);
			}

			char* leader_details[MAXSIZE];
			detokenize(recvBuff, leader_details, DELIMITER);
		}
	}

	while(1){	// PUT THE SWITCH CASE FOR TYPES OF MESSAGES HERE TO PERFORM THAT PARTICULAT OPERATION!
		// char sendBuff[MAXSIZE], recvBuff[MAXSIZE];

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
			strcpy(sendBuff, "JOINLEADER#127.0.0.1#1705");
			if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, other_addr_size) < 0){
				perror("ERROR: Sending message failed \n");
			}
		}
	} // end of while(1)
	return 0;
}