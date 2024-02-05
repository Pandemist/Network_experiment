#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <sys/time.h>
#include <map>
#include <pthread.h>
#include <mutex>
#include <iostream>
#include <time.h>

using namespace std;

#define SERVERPORT "8888"
#define CLIENTPORT "6666"
#define PEERPORT "7777"

#define BACKLOG 10

#define MAXTHREADS 3

#define TTL 30

#define MAXDATASIZE 50
#define HEADERSIZE 16

pthread_t threads[MAXTHREADS];
mutex mtx;
string argument;
bool sendAlive;

struct timeval tv;

/*
	Methodes requiers a sockaddr Struktur und gibt die
	IP V4 oder IP V6 Addresse des Sockets zurück.
*/
void *get_in_addr(struct sockaddr *sa) {
	if(sa->sa_family == AF_INET)  {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

string fillUntilFull(string str, int s) {
	while(str.size()<s) {
		str.append("0");
	}
	return str;
}

string createHeader(int seq, bool ack, bool syn, bool fin, bool res, bool end, bool inf, int type) {
	//HEADERSIZE 8 (SEQUNEZNUMMER), 6 (FLAGS), 1 (NULL-BITS), 1 (END OF HEADER CHAR)
	string flags = "";
	string header;
	string seqNum = to_string(seq);
 	ack ? flags.append("1") : flags.append("0");
	syn ? flags.append("1") : flags.append("0");
	fin ? flags.append("1") : flags.append("0");
	res ? flags.append("1") : flags.append("0");
	end ? flags.append("1") : flags.append("0");
	inf ? flags.append("1") : flags.append("0");
	while(seqNum.size()<7) {
		seqNum = seqNum.insert(0,"0");
	}
	header = seqNum.append(flags).append(to_string(type));
	return header;
}

bool sendThis(int socket, string msg) {
	cout<<"TestMessage: "<<msg<<endl;
	if(send(socket, (char *)(msg.c_str()), size_t(msg.size()), 0)<0) {
		fprintf(stderr,"Error in sending Message to server\n");
		sendAlive = false;
		return false;
	}
	return true;
}

bool mySender(int socket, string msg, int type) {
	int size = size_t(msg.size());
	int amoutOfSends = (size/(MAXDATASIZE-HEADERSIZE))+1;
	char msgArr = (char *)(msg.c_str());
	int sequenzNum = rand() % 99999999;
	string sendMsg = createHeader(sequenzNum,0,1,0,0,0,1,type)+"#";
	sendMsg = fillUntilFull(sendMsg, MAXDATASIZE);
	sendThis(socket, sendMsg);
	sequenzNum = ((sequenzNum+1)%100000000);
	int start = 0;
	int end = (MAXDATASIZE-HEADERSIZE);
	bool flag_end = false;
	if(size<end) {
		end = size;
		flag_end = true;
	}
	int i =0;
	while(i<amoutOfSends) {
//	while(end!=size) {
		sendMsg = createHeader(sequenzNum,1,0,0,0,flag_end,0,type)+"#"+msg.substr(start,(end-start));
//		cout<<start<<" - "<<end<<endl;
//		cout<<sendMsg<<endl;
		sendThis(socket, sendMsg);
		sequenzNum = ((sequenzNum+1)%100000000);
		start = end;
		end = ((MAXDATASIZE-HEADERSIZE) + end);
		if(size<end) {
			end = size;
			flag_end = true;
		}
		i++;
	}
}

void sigchld_handler(int s) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

int main(int argc, char const *argv[]) {
// variablendeklaration
	srand (time(NULL));
	signal(SIGPIPE, SIG_IGN);

	int sockfd;
	int numbytes;
	int yes = 1;
	int rv, rc;

	char buf[MAXDATASIZE];

	struct addrinfo hints;
	struct addrinfo *serverinfo;
	struct addrinfo *p;

	char s[INET6_ADDRSTRLEN];

	string peerIP;
	struct sigaction sa;

// Argument length handling
	if(argc != 2) {
		cout<<"Usage: ./client <serverIP>\n";
	}

// Timeout Timeval Initialisieren

	tv.tv_sec = TTL;
	tv.tv_usec = 0;

// prepare hints Structur (Recieve Socket)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((rv = getaddrinfo(argv[1], SERVERPORT, &hints, &serverinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

// loop through all the results and bind to the first we can
	for(p = serverinfo; p != NULL; p=p->ai_next) {
		if((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv,
				sizeof(struct timeval))) {
			perror("setsockopt");
			exit(1);
		}
		if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if(p == NULL) {
		fprintf(stderr, "client: failed to bind\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
		s, sizeof s);
	printf("client: connecting to server at %s\n", s);

	freeaddrinfo(serverinfo);

// Preparing server Messages
	char serverMsg[4];
	memset(serverMsg, '\0', 4);

// recieve SYN, or RES Statuscode from server
	if((rv = recv(sockfd, serverMsg, 4, 0)) <= 0) {
		cout<<"Failed to connect to server. Programm will exit..."<<endl;
		exit(1);
	}
	if(strcmp(serverMsg, "RES")==0) {
		cout<<"Server denied connection. Programm will exit..."<<endl;
		exit(1);
	}	
	if(strcmp(serverMsg, "SYN")==0) {
		cout<<"Connection sucessfully established."<<endl;
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	while(1) {
		cout<<"Gebe einen Text ein:"<<endl;
		string msg;
		string text = "Lorem ipsum dolor sit amet, consetetur sadipscing elitr, sed diam nonumy eirmod tempor invidunt ut labore et dolore magna aliquyam erat, sed diam voluptua. At vero eos et accusam et justo duo dolores et ea rebum. Stet clita kasd gubergren, no sea tak";
		getline(cin, msg);
		mySender(sockfd, text, 2);
/*		cout<<"Wie ist dein Name? -> (NAM)"<<endl;
		string msg;
		getline(cin, msg);
		if(send(sockfd, (char *)(msg.c_str()), size_t(msg.size()), 0)<0) {
			fprintf(stderr, "Error in sending request to Server\n");
		}

		char buf[MAXDATASIZE];
		memset(buf, '\0', MAXDATASIZE);
		if((rv = recv(sockfd, buf, MAXDATASIZE-1, 0))>0) {
			cout<<string(buf)<<endl;
		}else{
			cout<<"Error while recieving Message."<<endl;
		}*/
	}
}