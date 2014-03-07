/*
** netbounce-server.c
** from Beej's Guide to Network Programming: Using Unix Sockets, 
** by Brian "Beej" Hall.
**
** modified by Burt Rosenberg, 
** Created: Feb 8, 2009
** Last modified: Jan 30, 2013
*/

#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>

/* LOOP_FOREVER can be used to test that the socket is unconnected:
   it will answer any UDP packet arriving at MYPORT, unfiltered for
   source IP or port.
   LONG_BIND_SLEEP can be used to test that the recieve queue for
   MYPORT is available immediate after the bind, and to test queue depth.
*/

// #define LOOP_FOREVER 1
// #define LONG_BIND_SLEEP 1

#define MYPORT 3333

#define MAXBUFLEN 100

int main(void) {
	int sockfd;
	struct sockaddr_in my_addr;
	struct sockaddr_in their_addr;
	unsigned int addr_len, numbytes;
	char buf[MAXBUFLEN];
	
	if ((sockfd=socket(AF_INET,SOCK_DGRAM,0))==-1) {
		perror("socket") ;
		exit(1) ;
	}
	
	my_addr.sin_family = AF_INET ;
	my_addr.sin_port = htons(MYPORT) ;
	my_addr.sin_addr.s_addr = INADDR_ANY ;
	memset(&(my_addr.sin_zero),'\0',8) ;

	if (bind(sockfd, (struct sockaddr *)&my_addr, 
		sizeof(struct sockaddr)) == -1 ) {
		perror("bind") ;
		exit(1) ;
	}
	
#ifdef LONG_BIND_SLEEP
printf("going to sleep for 20 seconds ...") ;
fflush(NULL) ;
sleep(20) ;
printf("\nThat was refreshing. Continuing ... \n") ;
#endif

#ifdef LOOP_FOREVER
while (1==1 ) { /* while forever */
#endif

	addr_len = sizeof(struct sockaddr) ;
	if ((numbytes=recvfrom(sockfd, buf, MAXBUFLEN-1,0,
		(struct sockaddr *)&their_addr, &addr_len)) == -1 ) {
		perror("recvfrom") ;
		exit(1) ;
	}
	
	/* added to get source port number */
	printf("got packet from %s, port %d\n", inet_ntoa(their_addr.sin_addr), 
			ntohs(their_addr.sin_port)) ;
	printf("packet is %d bytes long\n", numbytes ) ;
	buf[numbytes] = '\0' ;
	printf("packet contains \"%s\"\n", buf ) ;
	
	{
	   /* return the packet to sender, 
	    */
		int bytes_sent ;
	    if ((bytes_sent = sendto( sockfd, buf, strlen(buf), 0,
	    		(struct sockaddr *)&their_addr, sizeof(struct sockaddr)))==-1 ) {
	    	perror("send") ;
	    	exit(1) ;
	    }
	    printf("packet sent, %d bytes\n", bytes_sent) ;
	}

#ifdef LOOP_FOREVER
}
#endif

	close(sockfd) ;
	return 0 ;
}
