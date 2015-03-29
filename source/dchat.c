#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
// #include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
// #include <ifaddrs.h>
// #include <string.h>
// #include <iostream>

// using namespace std;

#define PORT 1705
#define MAXSIZE 1024

int main(int argc, char* argv[]){
	int soc = 0, serv_addr_size, other_addr_size;
	struct sockaddr_in my_addr, serv_addr;
	struct sockaddr_un other_user_addr;
	char* mode;

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

		// BELOW CODE IS TO TRY AND FIND THE IP-ADDRESS SO THAT IT CAN BE PRINTED WHEN CLIENT STARTS NEW CHAT
		// char szBuffer[MAXSIZE];
		// gethostname(szBuffer, sizeof(szBuffer));
		// struct hostent *host = gethostbyname(szBuffer);
		// struct ifaddrs * ifAddrStruct = NULL;
		// struct ifaddrs * ifa = NULL;
		// void * tmpAddrPtr = NULL;

		// getifaddrs(&ifAddrStruct);
		// ifa = ifAddrStruct;
		// ifa = ifa->ifa_next;
		// tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
  //       char addressBuffer[MAXSIZE];
  //       inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, MAXSIZE);

		if(argc == 2){	
			/*
			START a new chat
			argv[1] = client name

			THIS FUNCTION AS OF NOW ALSO WAITS FOR ONE OTHER CLIENT TO JOIN, AND REPLIES TO THE CLIENT TO "JOIN THE LEADER"
			mode: WAITING (PROBABLY NEEDS RENAMING)
			other_user_addr: ADDRESS OF CLIENT TRYING TO JOIN THE CHAT
			*/
			mode = "WAITING";
			// string username = argv[1];
			// cout << username +" started a new chat on " << /*addressBuffer << ":" <<*/ PORT << endl;
			fprintf(stderr, "%s started a new chat on %d\n", argv[1], PORT);

			char sendBuff[] = "JOIN THE LEADER";
		    char recvBuff[MAXSIZE];

			if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, &other_addr_size) < 0){
				perror("Error: Receiving message failed \n");
			} else {
				fprintf(stderr, "%s\n", recvBuff);
			}

			if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&other_user_addr, other_addr_size) < 0){
				perror("ERROR: Sending message failed \n");
			}
		} else if(argc == 4){	
			/*
			JOIN an existing chat conversation
			argv[1] = client name
			argv[2] = IP-ADDRESS of the client already in the chat
			argv[3] = port number of the client already in the chat
			*/
			mode = "JOINING";
			// string username = argv[1], ip_addr = argv[2], port_no = argv[3];

			serv_addr.sin_family = AF_INET;
			serv_addr.sin_port = htons(atoi(argv[3]));

			if(inet_pton(AF_INET, argv[2], &serv_addr.sin_addr)<=0)
		    {
		        perror("ERROR: inet_pton error occured \n");
		        exit(-1);
		    }

		    char sendBuff[] = "JOIN";
		    char recvBuff[MAXSIZE];

		    if (sendto(soc, sendBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
				fprintf(stderr, "ERROR: Sorry no chat is active on %s, try again later. \n", argv[2]);
				exit(-1);
			}
			if(recvfrom(soc, recvBuff, MAXSIZE, 0, (struct sockaddr*)&serv_addr, &serv_addr_size) < 0){
				perror("ERROR: Receiving message failed \n");
			} else {
				fprintf(stderr, "%s \n", recvBuff);
			}
		}
	}
	return 0;
}