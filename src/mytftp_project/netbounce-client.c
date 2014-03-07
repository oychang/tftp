/*
** netbounce-client.c
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
#include<netdb.h>

#define MYPORT 3333

int main( int argc, char * argv[] ) {

	int sockfd ;
	struct sockaddr_in their_addr ;
	struct hostent *he ;
	int numbytes ;
	
	if ( argc!=3 ) {
		fprintf(stderr,"usage: netbounce-client hostname message\n") ;
		exit(1) ;
	}
	if ((he=gethostbyname(argv[1]))==NULL) {
		perror("gethostbyname") ;
		exit(1) ;
	}
	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1 ) {
		perror("socket") ;
		exit(1) ;
	}
	their_addr.sin_family = AF_INET ;
	their_addr.sin_port = htons(MYPORT) ;
	their_addr.sin_addr = *((struct in_addr *)he->h_addr) ;
	memset(&(their_addr.sin_zero), '\0', 8 ) ;
	
	if ((numbytes=sendto(sockfd, argv[2], strlen(argv[2]),0,
			(struct sockaddr *)&their_addr, sizeof(struct sockaddr)) ) == -1 ) {
		perror("sendto") ;
		exit(1) ;
	}
	printf("send %d bytes to %s\n", numbytes, inet_ntoa(their_addr.sin_addr)) ;
	
	/* added by burt;
	   sendto bound socket, let's get the port number that was selected
	*/
	{
	
#define MAXBUFLEN 100

		struct sockaddr_in my_addr ;
		unsigned int addr_len ;
		char buf[MAXBUFLEN];
		
		addr_len = sizeof(struct sockaddr_in) ;
		getsockname( sockfd, (struct sockaddr *)&my_addr, &addr_len ) ;
		printf("sent from port %d\n", ntohs(my_addr.sin_port)) ;
		/* on OSX 10.5.4, this prints out ascending port numbers each time run, 
		   starting around 49486 
		 */
		
		/* now receive the response .. question, will the packets buffer? after all
		   I am bound to the source port?
		 */
		 
		/*** put a wait in here to show how buffering worls ***/
		printf("sleeping for 5 seconds\n") ;
		sleep(5) ;
		
		printf("calling for return packet\n") ;
		addr_len = sizeof(struct sockaddr_in) ;
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
	
	}
	
	close(sockfd) ;
	return 0 ;
}

