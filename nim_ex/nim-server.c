/*
 * nim-server.c
 *
 *  Created on: Nov 21, 2014
 *      Author: tmayrav
 */
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h> // for open
#include <unistd.h>

typedef enum bool
{
	false = 0,
	true = 1
}bool;

typedef struct sockaddr_in1
{
	short sin_family; /* = AF_INET */
	u_short sin_port; /* port number */
	struct in_addr sin_addr;  /* 32-bit address */
	char sin_zero[8]; /* unused */
}sockaddr_in1_T;

typedef struct sockaddress
{
	u_short sa_family;
	char sa_data[14];
}sockaddr;

typedef struct Node{
	int moveStatus:1;//just switched from viewer to player
	int isPlayer :1;
	int illegalMove :1;
	int clientGameNum;
	int clientFD;
	struct Node *next;
}Node_T;

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

struct Node *head=NULL,*curr=NULL, *turn=NULL,*curr2=NULL, *curr3=NULL;

void computerMove (int * heaps){
	int max_i=0,i;
		for (i=1;i<4;i++){
			if (heaps[i]>=heaps[max_i]){
				max_i = i;
			}
		}
	heaps[max_i] -=1;
}

int legalMoveTest(int heaps[4],int index, int num){
	if ((index>3)||(index<0)) return 0;
	if (heaps[index]<num) return 0;
	if (num<1) return 0;
	return 1;
}

int isGameOver (int heaps[4]){
	bool B=true;
	int i;
	for(i=0;i<4;i++){
		if(heaps[i]!=0){
			B=false;
		}
	}
	return B;
}
void deleteSocket(const int *P,int *numOfActiveSockets,Node_T *delete,fd_set *writefds,fd_set *readfds,int *activeSockets,message_T *outMessage){
	Node_T *curr4;
	int i;
	if(-1==close(delete->clientFD)){
		printf("error closing fd %d because %s\n",delete->clientFD,strerror(errno));
	}
	FD_CLR(delete->clientFD,writefds);
	FD_CLR(delete->clientFD,readfds);
	activeSockets[delete->clientGameNum]=0;
	*numOfActiveSockets=*numOfActiveSockets-1;
	if(*numOfActiveSockets==0){
		free(delete);
		head=NULL;
	}
	if (*numOfActiveSockets>0){
		if(head==delete){
			head=head->next;
		}
		else{
			curr4=head;
			while(curr4->next!=delete){
				curr4=curr4->next;
			}
			curr4->next=delete->next;
		}
		if(delete->isPlayer && *numOfActiveSockets>=*P){
			curr4=head;
			for(i=0;i<*P-1;++i){
				curr4=curr4->next;
			}
			curr4->isPlayer=1;
			//send message to switched player
			outMessage->firstMessage=0;
			outMessage->yourTurn=(curr4==turn);
			outMessage->illegalMove=curr4->illegalMove;
			outMessage->joinedGame=1;//set for 1 if client SWITCHED to player
			outMessage->yourNum=curr4->clientGameNum;
			outMessage->Player=curr4->isPlayer;
			outMessage->type=1;
			outMessage->moveRejected=0;
			outMessage->moveStatus=0;
			//printf("sending message to inform of switch, client number = %d, type=%d , your turn=%d ,move rejected=%d \n",outMessage->yourNum,outMessage->type,outMessage->yourTurn,outMessage->moveRejected);
			if ((send(curr4->clientFD,outMessage,sizeof(message_T),0))<sizeof(message_T)){
				Node_T *delete2=curr4;
				printf("error sending message : %s\n",strerror(errno));
				deleteSocket(P,numOfActiveSockets,delete2,writefds,readfds,activeSockets,outMessage);
			}
		}
		if(turn==delete){
			do{
				turn=turn->next;
			}while(turn->isPlayer==0);
			outMessage->firstMessage=0;
			outMessage->yourTurn=1;
			outMessage->illegalMove=0;
			outMessage->joinedGame=0;//set for 1 if client SWITCHED to player
			outMessage->yourNum=turn->clientGameNum;
			outMessage->Player=turn->isPlayer;
			outMessage->type=1;
			outMessage->moveStatus=0;
			outMessage->moveRejected=0;
			//printf("sending message to inform of turn, client number = %d, type=%d , your turn=%d ,move rejected=%d \n",outMessage->yourNum,outMessage->type,outMessage->yourTurn,outMessage->moveRejected);
			if ((send(turn->clientFD,outMessage,sizeof(message_T),0))<sizeof(message_T)){
				Node_T *delete2=turn;
				printf("error sending message : %s\n",strerror(errno));
				deleteSocket(P,numOfActiveSockets,delete2,writefds,readfds,activeSockets,outMessage);
			}
		}
		free(delete);
	}
}
int main (int argsc, char** argsv){
	const int P = atoi(argsv[1]);
	const int M = atoi(argsv[2]);
	bool isMisere =atoi(argsv[3]),GameOver = false,legalMove=true;
	int port =6325,num,heapIndex;
	if (argsc==5){
			port=atoi(argsv[4]);
		}
	int numOfActiveSockets=0;
	int serverSocket;
	int activeSockets[9];
	memset(activeSockets,0,sizeof (activeSockets));
	int heaps [4];
	int i,clientWaiting,maxFd=0;
	fd_set readfds, writefds, curWrites,curReads;
	FD_ZERO(&readfds);
	FD_ZERO(&writefds);
	FD_ZERO(&curReads);
	FD_ZERO(&curWrites);
	for(i=0;i<4;i++){
		heaps[i]=M;
	}
	sockaddr_in1_T myaddr;
	sockaddr_in1_T clientaddr;
	myaddr.sin_family = AF_INET;
	myaddr.sin_port = htons( port );
	myaddr.sin_addr.s_addr = htonl( INADDR_ANY );
	message_T outMessage ,inMessage;
	memset(&outMessage,0,sizeof (message_T));
	memset(&inMessage,0,sizeof (message_T));
	outMessage.Misere = isMisere;
	outMessage.heapA = M;
	outMessage.heapB = M;
	outMessage.heapC = M;
	outMessage.heapD = M;
	outMessage.numPlayres=P;
	outMessage.firstMessage=1;
	outMessage.type=1;//0-MSG or 1- Move or 2- Quit from client side.  0-MSG 1-update from server side.
	memset(outMessage.MSG,0,sizeof (outMessage.MSG));
	memset(outMessage.MSG,0,sizeof (inMessage.MSG));
	serverSocket = socket(PF_INET, SOCK_STREAM, 0);
	if ((bind(serverSocket,(struct sockaddr*) &myaddr, sizeof(myaddr)))==-1){
		printf("error binding server socket : %s\n",strerror(errno));
		assert(!close(serverSocket));
		return errno;
	}
	if ((listen(serverSocket,1))==-1){
		printf("error with listen function : %s\n",strerror(errno));
		assert(!close(serverSocket));
		return errno;
	}
	FD_SET(serverSocket,&readfds);
	maxFd=serverSocket;
	socklen_t sin_size = sizeof(clientaddr);
	while(!GameOver){
		curReads=readfds;
		curWrites=writefds;
		select(maxFd+1,&curReads,&curWrites,NULL,NULL);
		clientWaiting=FD_ISSET(serverSocket,&curReads);
		while(clientWaiting){//join sockets that want to play
			Node_T * new = (Node_T *) malloc(sizeof(struct Node));
			new->illegalMove=0;
			new->moveStatus=0;
			new->clientFD = accept(serverSocket, (struct sockaddr*)&clientaddr, &sin_size);
			if(new->clientFD==-1){
				printf("error accepting client socket : %s\n",strerror(errno));
				free(new);
				continue;
			}
			if(numOfActiveSockets==9){//too many active players
				outMessage.tooManyPlayers=1;
				//printf("sending message to inform of too many players, too many players=%d \n",outMessage.tooManyPlayers);
				if ((send(new->clientFD,&outMessage,sizeof(message_T),0))<sizeof(message_T)) {
					printf("error sending message : %s\n",strerror(errno));

				}
				close(new->clientFD);
				free(new);
			}
			else{//there is room in the game
				for (i=0;i<9;i++){
					if(activeSockets[i]==0){
						activeSockets[i]=new->clientFD;
						new->clientGameNum=i;
						break;
					}
				}
				if(numOfActiveSockets<P){
					new->isPlayer=1;
				}
				else{
					new->isPlayer=0;
				}
				if(head==NULL){//first socket in linked list
					head=new;
					turn=new;
					new->next=head;
				}
				else{// put in linked list
					curr=head;
					while(curr->next!=head){
						curr=curr->next;
					}
					curr->next=new;
					new->next=head;
				}
				FD_SET(new->clientFD,&writefds);
				FD_SET(new->clientFD,&readfds);
				if(maxFd<new->clientFD) maxFd=new->clientFD;
				numOfActiveSockets++;
				outMessage.tooManyPlayers=0;
				outMessage.firstMessage=1;
				outMessage.yourTurn=(new==turn);
				outMessage.illegalMove=new->illegalMove;
				outMessage.joinedGame=0;//set for 1 if client SWITCHED to player
				outMessage.yourNum=new->clientGameNum;
				outMessage.Player=new->isPlayer;
				outMessage.type=1;
				outMessage.moveRejected=0;
				outMessage.moveStatus=0;
				//printf("sending message to inform of starting game, client number = %d, type=%d , your turn=%d ,move rejected=%d \n",outMessage.yourNum,outMessage.type,outMessage.yourTurn,outMessage.moveRejected);
				if ((send(new->clientFD,&outMessage,sizeof(message_T),0))<sizeof(message_T)) {
					printf("error sending message : %s\n",strerror(errno));
					deleteSocket(&P,&numOfActiveSockets,new,&writefds,&readfds,activeSockets,&outMessage);
				}
			}
			curReads=readfds;
			curWrites=writefds;
			select(maxFd+1,&curReads,&curWrites,NULL,NULL);
			clientWaiting=FD_ISSET(serverSocket,&curReads);
		}
		outMessage.tooManyPlayers=0;
		outMessage.firstMessage=0;
		if (head!=NULL){//start receiving messages
			curr=head;
			do{
				if(FD_ISSET(curr->clientFD,&curReads)){
					if (recv(curr->clientFD,(void *) &inMessage,sizeof(message_T),0)<sizeof(message_T)) {
						printf("error receiving message : %s\n",strerror(errno));
						deleteSocket(&P,&numOfActiveSockets,curr,&writefds,&readfds,activeSockets,&outMessage);
					}
					else{
						if(inMessage.type==0){//MSG
							outMessage.type=0;
							for(i=0;i<240;++i) outMessage.MSG[i]=inMessage.MSG[i];
							outMessage.sourceORdest=curr->clientGameNum;
							curReads=readfds;
							curWrites=writefds;
							select(maxFd+1,&curReads,&curWrites,NULL,NULL);//set time frame to see whos avilable
							if(inMessage.moveStatus==1){//send to everyone
								curr2=head;
								do{
									if(FD_ISSET(curr2->clientFD,&curWrites) && curr!=curr2){
										outMessage.yourNum=curr2->clientGameNum;
										//printf("sending MSG to everyone, client number = %d, type=%d , MSG=%s source=%d\n",outMessage.yourNum,outMessage.type,outMessage.MSG,outMessage.sourceORdest);
										if ((send(curr2->clientFD,&outMessage,sizeof(message_T),0))<sizeof(message_T)) {
											printf("error sending message : %s\n",strerror(errno));
											deleteSocket(&P,&numOfActiveSockets,curr2,&writefds,&readfds,activeSockets,&outMessage);
											break;
										}
									}
									curr2=curr2->next;
								}while(curr2!=head);
							}
							else{//send to someone specific
								curr2=head;
								do{
									if(curr2->clientGameNum==inMessage.sourceORdest&& curr!=curr2){
										if(FD_ISSET(curr2->clientFD,&curWrites)){
											outMessage.yourNum=curr2->clientGameNum;
											//printf("sending MSG to client number = %d, type=%d , MSG=%s source=%d\n",outMessage.yourNum,outMessage.type,outMessage.MSG,outMessage.sourceORdest);
											if ((send(curr2->clientFD,&outMessage,sizeof(message_T),0))<sizeof(message_T)) {
												printf("error sending message : %s\n",strerror(errno));
												deleteSocket(&P,&numOfActiveSockets,curr2,&writefds,&readfds,activeSockets,&outMessage);

											}
										}
										break;
									}
									curr2=curr2->next;
								}while(curr2!=head);
							}
						}
						if(inMessage.type==2){//Quit
							deleteSocket(&P,&numOfActiveSockets,curr,&writefds,&readfds,activeSockets,&outMessage);
						}
						if(inMessage.type==1){//Move
							if(curr!=turn){//it's not curr's turn
								outMessage.type=1;
								outMessage.moveRejected=1;
								//printf("sending message to inform of rejected move, client number = %d, type=%d your turn=%d,move rejected=%d \n",outMessage.yourNum,outMessage.type,outMessage.yourTurn,outMessage.moveRejected);
								if ((send(curr->clientFD,&outMessage,sizeof(message_T),0))<sizeof(message_T)) {
									printf("error sending message : %s\n",strerror(errno));
									deleteSocket(&P,&numOfActiveSockets,curr,&writefds,&readfds,activeSockets,&outMessage);
								}
							}
							else{//it is curr's turn
								curr->moveStatus=1;
								num = inMessage.heapD;
								heapIndex = inMessage.heapIndex;
								legalMove = legalMoveTest(heaps,heapIndex,num);
								if (legalMove){
									heaps[heapIndex] -=num;
									outMessage.heapA = heaps[0];
									outMessage.heapB = heaps[1];
									outMessage.heapC = heaps[2];
									outMessage.heapD = heaps[3];
								}
								else{
									curr->illegalMove =1;
								}
								GameOver = isGameOver(heaps);
								if(GameOver){
									outMessage.GameProgress=1;
									outMessage.moveRejected=0;
									outMessage.type=1;
									curr2=head;
									do{
										outMessage.Player=curr2->isPlayer;
										//printf("ID =  = %d, player = %d , winLose = %d, moveStatus = %d, type  = %d, move rejeceted = %d,  game progress = %d\n",outMessage.yourNum,outMessage.Player,outMessage.youLoseORyouWin,outMessage.moveStatus,outMessage. );
										outMessage.yourNum=curr2->clientGameNum;
										if ((!isMisere &&curr==curr2)||(isMisere && curr!=curr2)){
											outMessage.youLoseORyouWin =1;
										}
										if((isMisere && curr==curr2)||(!isMisere && curr!=curr2)) {
											outMessage.youLoseORyouWin =0;
										}
										outMessage.moveStatus=curr2->moveStatus;
										//printf("sending message to inform of game ending, client number = %d, type=%d ,game progress=%d \n",outMessage.yourNum,outMessage.type,outMessage.GameProgress);
										if ((send(curr2->clientFD,&outMessage,sizeof(message_T),0))<sizeof(message_T)) {
											printf("error sending message : %s\n",strerror(errno));
											deleteSocket(&P,&numOfActiveSockets,curr2,&writefds,&readfds,activeSockets,&outMessage);
											break;
										}
										curr2=curr2->next;
									}while(curr2!=head);
									assert(!close(serverSocket));
									curr3=head;
									if (numOfActiveSockets==1) free (head);
									else{
										for (i=0;i<numOfActiveSockets-1;++i){
											curr3=head->next;
											free(head);
											head=curr3;
										}
										free (head);

									}
									for (i=0;i<9;++i){
										if(activeSockets[i]!=0){
											assert(!close(activeSockets[i]));
										}
									}
									return 0;
								}
								else{//game still in progress
									do{
										turn=turn->next;
									}while(turn->isPlayer==0);
									curr2=head;
									do{
										outMessage.moveStatus=curr2->moveStatus;
										outMessage.yourTurn=(curr2==turn);
										outMessage.illegalMove=curr2->illegalMove;
										outMessage.joinedGame=0;//set for 1 if client SWITCHED to player
										outMessage.yourNum=curr2->clientGameNum;
										outMessage.Player=curr2->isPlayer;
										outMessage.type=1;
										outMessage.moveRejected=0;
										//printf("sending message after player used turn, client number = %d, type=%d your turn=%d,move rejected=%d,illegal move=%d \n",outMessage.yourNum,outMessage.type,outMessage.yourTurn,outMessage.moveRejected, outMessage.illegalMove);
										if ((send(curr2->clientFD,&outMessage,sizeof(message_T),0))<sizeof(message_T)) {
											printf("error sending message : %s\n",strerror(errno));
											deleteSocket(&P,&numOfActiveSockets,curr2,&writefds,&readfds,activeSockets,&outMessage);
											break;
										}
										curr2=curr2->next;
									}while(curr2!=head);
									curr->moveStatus=0;
								}
							}
						}
					}
				}
				if (inMessage.type!=2) curr=curr->next;
			}while(inMessage.type !=2 && curr!=head && !GameOver && head!=NULL);
		}
	}
	assert(!close(serverSocket));
	if (numOfActiveSockets==1) free (head);
	else{
		for (i=0;i<numOfActiveSockets-1;++i){
			curr3=head->next;
			free(head);
			head=curr3;
		}
		free (head);

	}
	for (i=0;i<9;++i){
		if(activeSockets[i]!=0){
			close(activeSockets[i]);
		}
	}
	return 0;
}

