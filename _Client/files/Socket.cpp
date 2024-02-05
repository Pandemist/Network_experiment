//Socket.cpp
socket::setupNewSocket(String addr, int port, int returnMessage) {
	int sockfd;
	
	struct addrinfo hints;
	struct addrinfo *serverinfo;
	struct addrinfo *p;
	
	char s[INET6_ADDRSTRLEN];
	
	
// prepare hints Structur (Recieve Socket)
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if((rv = getaddrinfo(addr, port, &hints, &serverinfo)) != 0) {
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

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to server at %s\n", s);
	
	return sockfd;
}