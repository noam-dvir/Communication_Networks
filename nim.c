/*
 * nim.c

 *
 *  Created on: Nov 21, 2014
 *      Author: noam
 */

#include <string.h>
#include<stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <assert.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

int getaddrinfo(const char *nodename, const char *servname,const struct addrinfo *hints, struct addrinfo **res);

void freeaddrinfo(struct addrinfo *ai);

const char *gai_strerror(int ecode);



typedef struct sockaddress
{
	u_short sa_family;
	char sa_data[14];
}sockaddr;


typedef struct sockaddress_in
{
	short sin_family; /* = AF_INET */
	u_short sin_port; /* port number */
	struct in_addr sin_addr;  /* 32-bit address */
	char sin_zero[8]; /* unused */
}sockaddr_in;


typedef struct in_address
{
	uint32_t  s_addr;
}in_addr;



#pragma pack(push, 1)
typedef struct message
{
	 unsigned int Misere : 1;
	 unsigned int heapA : 11;
	 unsigned int heapB : 11;
	 unsigned int heapC : 11;
	 unsigned int heapD : 11;
	 unsigned int moveStatus:1;//1 after move;
	 unsigned int firstMessage :1;
	 unsigned int GameProgress : 1;//0- game in progress, 1- game ended
	 unsigned int illegalMove : 1;
	 unsigned int youLoseORyouWin : 1;//0- you lose ,1- you win
	 unsigned int heapIndex : 2;
	 unsigned int Player : 1;//are you a player or a viewer
	 unsigned int yourTurn : 1;//is it your turn
	 unsigned int numPlayres : 4;//total number of players
	 unsigned int yourNum : 4;//client number
	 unsigned int joinedGame : 1;//just turned from viewer to player
	 unsigned int tooManyPlayers : 1;
	 unsigned int type : 2;//0-MSG or 1- Move or 2- Quit from client side.  0-MSG 1-update from server side.
	 char MSG[240];
	 unsigned int sourceORdest :4;//if sent from client indicates who we need to send in to, if sent from server indicates who it was sent from
	 unsigned int moveRejected:1;
}message_T;
#pragma pack(pop)


typedef enum bool
{
	false = 0,
	true = 1
}bool;



int main (int argsc, char** argsv){
	int sock1,x,t1,t2,j;
	int port =6325,i1,i2,k,count,len;
	char buffer [5];

	char * hostname = "localhost";
	if (argsc==3){
		hostname = argsv[1];
		port=atoi(argsv[2]);
	}
	sprintf(buffer, "%d", port);
	bool GameEnd = false,EndLoop,wrongInput;
	char str[300];
	struct addrinfo hints, *servinfo, *p;
	int rv;

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET; // use AF_INET6 to force IPv6
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(hostname, buffer, &hints, &servinfo)) != 0) {
	    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
	    exit(1);
	}

	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
	    if ((sock1 = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
	        perror("socket");
	        continue;
	    }

	    if (connect(sock1, p->ai_addr, p->ai_addrlen) == -1) {
	    	assert (!close(sock1));
	        perror("connect");
	        continue;
	    }

	    break; // if we get here, we must have connected successfully
	}

	if (p == NULL) {
	    // looped off the end of the list with no connection
	    fprintf(stderr, "failed to connect\n");
	    exit(2);
	}

	freeaddrinfo(servinfo); // all done with this structure
	message_T data1;
	data1.Misere=0;
	data1.heapIndex=0;

	fd_set Rfdset;
	fd_set set2;
	FD_ZERO(&Rfdset);



	while (GameEnd==false){

		FD_SET(sock1,&Rfdset);
		FD_SET(0,&Rfdset);

		select(sock1+1,&Rfdset,NULL,NULL,NULL);

		if(FD_ISSET(0,&Rfdset)){

			len=read(0, str,240);
			str[len]='\0';


			wrongInput=true;
			if(str[0]=='M' && str[1]=='S' && str[2]=='G' && str[3]==' '){//message
				data1.type=0;
				if(str[4]=='-' && str[5=='1']){
					data1.moveStatus=1;
				}
				else {
					data1.moveStatus = 0;
				}
				data1.sourceORdest = atoi(str+3);


				i1 = (data1.sourceORdest>=10) ? 6:5;
				i2=0;
				while (str[i1]!='\0'){
					data1.MSG[i2] = str[i1+1];
					i1++;i2++;

				}
				data1.sourceORdest--;

				wrongInput=false;


			}
			if (str[0]=='Q' ){//quit
				data1.type=(unsigned int)2;
				x = send(sock1, &data1, sizeof(message_T),0);
				if (x==0){
					printf("Disconnected from server\n");
					assert(!close(sock1));
					return errno;
				}
				if(x<sizeof(message_T)){
					perror("Error sending message :");
					assert(!close(sock1));
					return errno;
				}
				assert(!close(sock1));
				return 0;
			}

			if(str[0]>='A'&&str[0]<='D' && str[1]==' ' && atoi(str+2)>0 && atoi(str+2)<1500 ){//move
				data1.heapIndex = str[0]-'A';
				data1.heapD = atoi(str+2);
				data1.type=1;
				wrongInput=false;

			}
			if (wrongInput==true){
				data1.heapIndex = 0;
				data1.heapD = 0;
				data1.type = 1;

			}

			x = send(sock1, &data1, sizeof(message_T),0);
			if (x==0){
				printf("Disconnected from server\n");
				assert(!close(sock1));
				return errno;
			}
			if(x<sizeof(message_T)){
				perror("Error sending message :");
				assert(!close(sock1));
				return errno;
			}
			continue;


		}



		if (FD_ISSET(sock1,&Rfdset)){

			x=recv(sock1,(void *) &data1,sizeof(message_T),0);

			if (x==0){
				printf("Disconnected from server\n");
				assert(!close(sock1));
				return errno;
			}
			if(x==-1){
				perror("Error receiving message:");
				assert(!close(sock1));
				return errno;
			}
			if (data1.type == 1){
				if (data1.tooManyPlayers==1){
					printf("Client rejected: too many clients are already connected\n");
					return 0;
					break;
				}
				if (data1.moveRejected ==1){
					printf("Move rejected: this is not your turn\n");
					continue;
				}
				if (data1.joinedGame){
					printf("You are now playing!\n");
				}

				if (data1.firstMessage && data1.Misere){       //first turn, misere game
					printf("This is a Misere game\n");
				}
				if (data1.firstMessage && !(data1.Misere)){
					printf("This is a Regular game\n");  //first turn, regular game
				}
				if (data1.firstMessage ){
					printf("Number of players is %d\n", data1.numPlayres);
					printf("You are client %d\n", data1.yourNum+1);
				}
				if (data1.firstMessage && data1.Player==1){
					printf("You are playing\n");
				}
				if (data1.firstMessage && data1.Player==0){
					printf("You are viewing\n");
				}
				if (data1.moveStatus &&  !(data1.illegalMove)){//game in progress,illegalmove==0
					printf("Move accepted\n");
				}
				if (data1.moveStatus && data1.illegalMove){//game in progress,illegalmove==1
					printf("Illegal move\n");
				}
				printf ("Heap sizes are %d, %d, %d, %d\n",data1.heapA,data1.heapB,data1.heapC,data1.heapD);
				if (data1.Player && data1.GameProgress && !(data1.youLoseORyouWin)){ //game over, "computer" wins
					printf("You lose!\n");
					GameEnd=true;
					return 0;
					continue;
				}
				if (data1.Player && data1.GameProgress && data1.youLoseORyouWin){  //game over,client wins
					printf("You win!\n");
					GameEnd=true;
					return 0;
					continue;
				}
				if (!(data1.Player) && data1.GameProgress){
					printf("Game over!\n");
					return 0;
				}

				if (data1.Player==1 && data1.yourTurn){
					printf ("Your turn:\n");
				}
				EndLoop=false;
				count = 1;
				while (EndLoop == false && data1.yourTurn){

					FD_ZERO(&set2);
					FD_SET(sock1,&set2);
					FD_SET(0,&set2);
					select(sock1+1,&set2,NULL,NULL,NULL);
					if (FD_ISSET(sock1,&set2)){
						x=recv(sock1,(void *) &data1,sizeof(message_T),0);

						if (x==0){
							printf("Disconnected from server\n");
							assert(!close(sock1));
							return errno;
						}
						if(x==-1){
							perror("Error receiving message:");
							assert(!close(sock1));
							return errno;
						}



					}
					if (FD_ISSET(0, &set2)){
						len=read(0,str,240);
						str[len]='\0';
						wrongInput=true;
						if(str[0]=='M' && str[1]=='S' && str[2]=='G' && str[3]==' '){//message
							data1.type=0;
							if(str[4]=='-' && str[5=='1']){
								data1.moveStatus=1;
							}
							else{
								data1.moveStatus=0;
							}
							data1.sourceORdest = atoi(str+3);
							i1 = (data1.sourceORdest>=10) ? 6:5;
							i2=0;
							while (str[i1]!='\0'){
								data1.MSG[i2] = str[i1+1];
								i1++;i2++;

							}
							wrongInput=false;
							data1.sourceORdest--;

						}
						if (str[0]=='Q' ){//quit
							data1.type=(unsigned int)2;
							x = send(sock1, &data1, sizeof(message_T),0);
							if (x==0){
								printf("Disconnected from server\n");
								assert(!close(sock1));
								return errno;
							}
							if(x<sizeof(message_T)){
								perror("Error sending message :");
								assert(!close(sock1));
								return errno;
							}
							assert(!close(sock1));
							return 0;
						}
						if(str[0]>='A'&&str[0]<='D' && str[1]==' ' && atoi(str+2)>0 && atoi(str+2)<1500 ){//move
							data1.heapIndex = str[0]-'A';
							data1.heapD = atoi(str+2);
							data1.type=1;
							wrongInput=false;
							EndLoop=true;
						}
						if (wrongInput==true){
							data1.heapIndex = 0;
							data1.heapD = 0;
							data1.type = 1;
							EndLoop=true;

						}
						x = send(sock1, &data1, sizeof(message_T),0);
						if (x==0){
							printf("Disconnected from server\n");
							assert(!close(sock1));
							return errno;
						}
						if(x<sizeof(message_T)){
							perror("Error sending message :");
							assert(!close(sock1));
							return errno;
						}
						t1 = (EndLoop==true) ? 1:0;
					}
					count++;

				}



			}
			if (data1.type ==0){
				printf("%d: ",data1.sourceORdest+1);
				k=0;
				while (data1.MSG[k]!='\0'){
					printf("%c",data1.MSG[k]);
					k++;
				}
				continue;

			}


		}




	}

	assert (!close(sock1));
	return 1;
}
