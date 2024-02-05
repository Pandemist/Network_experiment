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
#include <csignal>
#include <queue>
#include <fcntl.h>

using namespace std;

#define PORT "8888" //DEFAULT Recieve Port

#define TTL 30

#define BACKLOG 10

#define MAXTHREADS 1000 //MAX Amount of open Connections to the Server

#define MAXDATASIZE 50

struct info {
	int sock_fd;
	string ip;
};

struct rcvd {
	int art;
	info sndrSock;
	string msg;
};

queue<rcvd> recievedMsgQueue;
map<string, info> clientMap;
map<string, int> threadMap;
pthread_t threads[MAXTHREADS];
mutex mtx;

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

void sigchld_handler(int s) {
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

void sendToClient(int socket, string msg) {
	send(socket, (char *)(msg.c_str()), size_t(msg.size()), 0);
}

string getDataPath(string msg) {
	return msg.substr(msg.find("#") + 1);
}

string getHeaderPath(string msg) {
	return msg.substr(0, msg.find("#"));
}
int getSequenzNum(string header) {
	return stoi(header.substr(0, 8));
}
string getFlags(string header) {
	return header.substr(8, 6);
}

void *threadForClient(void* s) {
	int rv;
	string client_ip = (char*)s;
	char buf[MAXDATASIZE];
	memset(buf, '\0', MAXDATASIZE);


	while((rv = recv(clientMap[client_ip].sock_fd, buf, MAXDATASIZE, 0))>0) {
		string header = getHeaderPath(buf);
		string data = getDataPath(buf);
		int seqNum = getSequenzNum(header);
		string flags = getFlags(header);
		if(flags[5]=='1') {
			//Handle first line
		}
		cout<<flags[0]<<"-"<<flags[1]<<"-"<<flags[2]<<"-"<<flags[3]<<"-"<<flags[4]<<"-"<<flags[5]<<endl;
		if(flags[4]=='1') {
			//Handle last line
		}











		cout<<clientMap[client_ip].ip<<": sends \""<<getHeaderPath(buf)<<" - "<<getDataPath(buf)<<endl;
		memset(buf, '\0', MAXDATASIZE);
/*	if(strcmp(buf, "NAM")==0) {
			sndMsg = "Mein Name ist Emil";
		}else{
			sndMsg = "Ich hab die frage nicht verstanden!";
		}
		sendToClient(clientMap[client_ip].sock_fd, sndMsg);*/
	}
}

int main(int argc, char const *argv[]) {
// variablendeklaration

	signal(SIGPIPE, SIG_IGN);

	int sockfd;
	int new_fd;
	int yes = 1;
	int rv, rc;
	int threadCounter=0;

	struct addrinfo hints;
	struct addrinfo *serverinfo;
	struct addrinfo *p;

	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;

	char s[INET6_ADDRSTRLEN];
	string client_ip;

// Timeout Timeval Initialisieren

	tv.tv_sec = TTL;
	tv.tv_usec = 0;

// prepare hints Structur (Recieve Socket)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if((rv = getaddrinfo(NULL, PORT, &hints, &serverinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}

// loop through all the results and bind to the first we can
	for(p = serverinfo; p != NULL; p=p->ai_next) {
		if((sockfd = socket(p->ai_family, p->ai_socktype,
				p->ai_protocol)) == -1) {
			perror("server: socket");
			continue;
		}
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
				sizeof(int)) == -1) {
			perror("setsockopt");
			exit(1);
		}
		if(setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv,
				sizeof(struct timeval))) {
			perror("setsockopt");
			exit(1);
		}
		if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	if(p == NULL) {
		fprintf(stderr, "server: failed to bind\n");
		return 2;
	}

	freeaddrinfo(serverinfo);

	if(listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	sa.sa_handler = sigchld_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if(sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}

	printf("server: waiting for connection...\n");
	//TODO: Connectionhandling
	while(1) {
		sin_size = sizeof their_addr;
		new_fd = accept(sockfd, (struct sockaddr *)&their_addr,
			&sin_size);
		if(new_fd == -1) {
		//	perror("accept");
			continue;
		}
		inet_ntop(their_addr.ss_family,
			get_in_addr((struct sockaddr *)&their_addr),
			s, sizeof s);
		printf("server: got connection from %s\n", s);

		client_ip = s;

		if(threadCounter<MAXTHREADS) {
			threadMap[client_ip]=threadCounter++;
			if(send(new_fd, "SYN", 4, 0) == -1) {
				perror("send");
			}
		}else{
			cout<<"Server ran out of threads"<<endl;
			if(send(new_fd, "RES", 4, 0) == -1) {
				perror("send");
			}
			continue;
		}

		mtx.lock();

		clientMap[client_ip].ip = client_ip;
		clientMap[client_ip].sock_fd = new_fd;

		mtx.unlock();

		if((rc = pthread_create(&threads[threadMap[client_ip]], NULL, threadForClient, (void*)s))!=0) {
			fprintf(stderr, "Error: unable to create thread, %d\n", rc);
			close(clientMap[client_ip].sock_fd);
			clientMap.erase(client_ip);
		}

	}
	return 0;
}