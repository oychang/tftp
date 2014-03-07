#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "Logging.h"

#define MYPORT 3333 // Change to 3335 for real implementation
#define MAXBUFLEN 516
#define MAXDATALEN 512
#define MAXMODELEN 8
#define OPCODE_RRQ "01"
#define OPCODE_WRQ "02"
#define OPCODE_DAT "03"
#define OPCODE_ACK "04"

int main(int argc, char *argv[]) {

  int sockfd;
  struct sockaddr_in their_addr;
  struct hostent *he;
  struct sockaddr_in my_addr;
  unsigned int addr_len;
  char sendbuf[MAXBUFLEN];
  char recvbuf[MAXBUFLEN];
  char mode[MAXMODELEN] = "octet";
  int numbytes;
  int vflag = 0;
  int rflag = 0;
  int wflag = 0;
  char *pvalue = NULL;
  char *file_name = NULL;
  char *host_name = NULL;
  int index;
  int argument;
  int bufferPos;
  int loopcond = 1;
  int block_number;
  char temp_blno[2];
  FILE *ioFile;
  char *sentinel;
  char fileLine[MAXDATALEN];

  while ((argument = getopt(argc, argv, "vrwp:")) != -1) {
    switch (argument) {
      case 'v':
        VERBOSE = 1;
        break;
      case 'r':
        rflag = 1;
        break;
      case 'w':
        wflag = 1;
        break;
      case 'p':
        pvalue = optarg;
        break;
      default:
        abort();
    }
  }
  file_name = argv[optind];
  host_name = argv[optind + 1];

  if ((he = gethostbyname(host_name)) == NULL) {
    perror("gethostbyname");
    exit(1);
  }
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  their_addr.sin_family = AF_INET;
  if(strcmp(argv[1], "-p") == 0) {
    their_addr.sin_port = htons(atoi(pvalue));
  } else {
    their_addr.sin_port = htons(MYPORT);
  }
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(&(their_addr.sin_zero), '\0', 8);

  // Pack and send the initial read/write request; establish connection
  // if rflag is set, opcode 01; if wflag is set, opcode 02
  bufferPos = 0;
  sendbuf[bufferPos] = '\0'; // Clear buffer by placing '\0' at index 0
  if (rflag) {
    if ((ioFile = fopen(file_name, "w")) == NULL) {
      perror("opening local file for writing");
      exit(1);
    }
    strncat(sendbuf, OPCODE_RRQ, 2);
    bufferPos = 2;
  } else if (wflag) {
    if ((ioFile = fopen(file_name, "r")) == NULL) {
      perror("opening local file for reading");
      exit(1);
    }
    strncat(sendbuf, OPCODE_WRQ, 2);
    bufferPos = 2;
  } else {
    printf("ERROR: No r/w designation given.\n");
    exit(1);
  }

  // Add the target file name into the buffer
  strcat(sendbuf, file_name);
  bufferPos += strlen(file_name);

  // Place 1 byte containing zero, the mode, then another byte of zero
  sendbuf[bufferPos] = '0';
  bufferPos++;
  strcat(sendbuf, mode);
  bufferPos += strlen(mode);
  sendbuf[bufferPos] = '0';
  bufferPos++;

  printf("The packet so far: ");
  for (index = 0; index < bufferPos; index++) {
    printf("%c", sendbuf[index]);
  }
  printf("\n");

  if ((numbytes = sendto(sockfd, sendbuf, bufferPos, 0, 
			 (struct sockaddr *)&their_addr, 
			 sizeof(struct sockaddr))) == -1) {
    perror("sendto");
    exit(1);
  }
  printf("Send %d bytes to %s\n", numbytes, 
	 inet_ntoa(their_addr.sin_addr));

  addr_len = sizeof(struct sockaddr_in);
  getsockname(sockfd, (struct sockaddr *)&my_addr, &addr_len);
  printf("Sent from port %d\n", ntohs(my_addr.sin_port));
  
  //  printf("Sleeping for 5 seconds\n");
  //  sleep(5);

  if (rflag) {
    block_number = 1;
    while (loopcond) {
      printf("Calling for return packet\n");
      addr_len = sizeof(struct sockaddr_in);
      if ((numbytes = recvfrom(sockfd, recvbuf, MAXBUFLEN - 1, 0,
	  (struct sockaddr *)&their_addr, &addr_len)) == -1) {
	perror("recvfrom");
        exit(1);
      }
      printf("Got packet from %s, port %d\n",
             inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
      printf("Packet is %d bytes long\n", numbytes);
      recvbuf[numbytes] = '\0';
      printf("Packet contains \"%s\"\n", recvbuf);
      if (numbytes < 516) {
	loopcond = 0;
      }
      if (recvbuf[0] == 0 && recvbuf[1] == 3) {
        strncpy(temp_blno, &recvbuf[2], 2);
	if (atoi(temp_blno) == block_number) {
          printf("New received data:\n%s\n", &recvbuf[4]);
	  // Have to put the new received data into local file
	  if(fputs(&recvbuf[4], ioFile) != EOF) {
            printf("Successfully wrote 512 bytes to file\n");
          } else {
            perror("write to local file");
            exit(1);
          }
          // Construct an acknowledgement packet and send back
          sendbuf[0] = '\0';
          strcat(sendbuf, OPCODE_ACK);
          sendbuf[2] = (char)(block_number / 10 + 48);
          sendbuf[3] = (char)(block_number % 10 + 48);
          sendbuf[4] = '\0';
	  if ((numbytes = sendto(sockfd, sendbuf, 4, 0, 
	      (struct sockaddr *)&their_addr,
              sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
	  }
	  printf("Send %d bytes to %s\n", numbytes, 
	    inet_ntoa(their_addr.sin_addr));
          block_number++;
        }
      }
    }
  } else if (wflag) {
    block_number = 0;
    while (loopcond) {
      printf("Calling for return packet\n");
      addr_len = sizeof(struct sockaddr_in);
      if ((numbytes = recvfrom(sockfd, recvbuf, MAXBUFLEN - 1, 0, 
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
      }
      printf("Got packet from %s, port %d\n", 
              inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
      printf("Packet is %d bytes long\n", numbytes);
      recvbuf[numbytes] = '\0';
      printf("Packet contains \"%s\"\n", recvbuf);
      if (strncmp(recvbuf, OPCODE_ACK, 2) == 0) {
        if (recvbuf[2] == (char)(block_number / 10 + 48) &&
            recvbuf[3] == (char)(block_number % 10 + 48)) {
          printf("New received ack\n");
          block_number++;
          sendbuf[0] = '\0';
          strcat(sendbuf, OPCODE_DAT);
          sendbuf[2] = (char)(block_number / 10 + 48);
          sendbuf[3] = (char)(block_number % 10 + 48);
	  bufferPos = 4;
	  // Add the data to the packet!
          if (fgets(fileLine, MAXDATALEN, ioFile) != NULL) {
	    sentinel = fgets(fileLine, MAXDATALEN, ioFile);
	    fileLine[strlen(fileLine) -1] = '\0';
	    bufferPos += strlen(fileLine);
            strcat(sendbuf, fileLine);
	  } else {
	    perror("reading from local file");
	    exit(1);
	  }
          /*sentinel = fgets(fileLine, MAXDATALEN, ioFile);
	  fileLine[strlen(fileLine) - 1] = '\0';
          bufferPos += strlen(fileLine);
          if (sentinel != NULL) {
            strcat(sendbuf, fileLine);
	    } */
	  if ((numbytes = sendto(sockfd, sendbuf, bufferPos, 0, 
	      (struct sockaddr *)&their_addr, 
	      sizeof(struct sockaddr))) == -1) {
	    perror("sendto");
	    exit(1);
	  }
          printf("send %d bytes to %s\n", numbytes, 
	    inet_ntoa(their_addr.sin_addr));
          if (numbytes < 516) {
            loopcond = 0;
	  }
        }
      }
    }
  }
  if (fclose(ioFile) == EOF) {
    perror("closing local file");
    exit(1);
  }
  close(sockfd);
  return 0;
}
