#include "client.h"

int tftp_client(int port, int rflag, char *file_name, char *host_name) {

  int sockfd;
  struct sockaddr_in their_addr;
  struct hostent *he;
  struct sockaddr_in my_addr;
  unsigned int addr_len;
  char sendbuf[MAXBUFLEN];
  char recvbuf[MAXBUFLEN];
  char mode[MAXMODELEN] = "octet";
  int numbytes;
  int wflag = (rflag ? 0 : 1);
  // int index;
  // int argument;
  int bufferPos;
  int loopcond = 1;
  int block_number;
  //  char temp_blno[2];
  FILE *ioFile;
  char fileLine[MAXDATALEN];

  if ((he = gethostbyname(host_name)) == NULL) {
    perror("gethostbyname");
    exit(1);
  }
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  their_addr.sin_family = AF_INET;
  their_addr.sin_port = htons(port);
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(&(their_addr.sin_zero), '\0', 8);

  // Pack and send the initial read/write request; establish connection
  // if rflag is set, opcode 01; if wflag is set, opcode 02
  bufferPos = 0;
  if (rflag) {
    if ((ioFile = fopen(file_name, "w")) == NULL) {
      perror("opening local file for writing");
      exit(1);
    }
    memcpy(sendbuf, (char [2]){0, 1}, 2*sizeof(char));
    // strncat(sendbuf, OPCODE_RRQ, 2);
    bufferPos += 2;
  } else {
    if ((ioFile = fopen(file_name, "r")) == NULL) {
      perror("opening local file for reading");
      exit(1);
    }
    memcpy(sendbuf, (char [2]){0, 2}, 2*sizeof(char));
    // strncat(sendbuf, OPCODE_WRQ, 2);
    bufferPos += 2;
  }

  // Add the target file name into the buffer
  strcpy(&sendbuf[bufferPos], file_name);
  bufferPos += strlen(file_name);
  sendbuf[bufferPos] = '\0';
  bufferPos++;

  strcpy(&sendbuf[bufferPos], mode);
  bufferPos += strlen(mode);
  sendbuf[bufferPos] = '\0';
  bufferPos++;

  /* printf("The packet so far: ");
  for (index = 0; index < bufferPos; index++) {
    printf("%c", sendbuf[index]);
  }
  printf("\n"); */

  if ((numbytes = sendto(sockfd, sendbuf, bufferPos, 0,
			 (struct sockaddr *)&their_addr,
			 sizeof(struct sockaddr))) == -1) {
    perror("sendto");
    exit(1);
  }
  log("Send %d bytes to %s\n", numbytes,
	 inet_ntoa(their_addr.sin_addr));
  addr_len = sizeof(struct sockaddr_in);
  getsockname(sockfd, (struct sockaddr *)&my_addr, &addr_len);
  log("Sent from port %d\n", ntohs(my_addr.sin_port));

  //  printf("Sleeping for 5 seconds\n");
  //  sleep(5);

  if (rflag) {
    block_number = 1;
    while (loopcond) {
      // log("Calling for return packet\n");
      addr_len = sizeof(struct sockaddr_in);
      if ((numbytes = recvfrom(sockfd, recvbuf, MAXBUFLEN - 1, 0,
	  (struct sockaddr *)&their_addr, &addr_len)) == -1) {
	perror("recvfrom");
        exit(1);
      }
      log("Got packet from %s, port %d\n",
             inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
      log("Packet is %d bytes long\n", numbytes);
      recvbuf[numbytes] = '\0';
      log("Packet contains \"%s\"\n", recvbuf);
      if (numbytes < MAXBUFLEN) {
	loopcond = 0;
      }
      if (recvbuf[0] == 0 && recvbuf[1] == 3) {
        if ((recvbuf[2] == block_number / 10) &&
	    (recvbuf[3] == block_number % 10)) {
          log("New received data: %s\n", &recvbuf[4]);
	  // Have to put the new received data into local file
	  if(fputs(&recvbuf[4], ioFile) != EOF) {
            log("Successfully wrote new data to file\n");
          } else {
            log("write to local file");
            exit(1);
          }
          // Construct an acknowledgement packet and send back
          sendbuf[0] = '\0';
          memcpy(sendbuf, (char [4]){0, 4, block_number / 10, 
		 block_number % 10}, 4*sizeof(char));
          //sendbuf[2] = block_number / 10 + 48;
          //sendbuf[3] = block_number % 10 + 48;
          bufferPos = 4;
	  if ((numbytes = sendto(sockfd, sendbuf, bufferPos, 0,
	      (struct sockaddr *)&their_addr,
              sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
	  }
	  log("Send %d bytes to %s\n", numbytes,
	    inet_ntoa(their_addr.sin_addr));
	  addr_len = sizeof(struct sockaddr_in);
	  getsockname(sockfd, (struct sockaddr *)&my_addr, &addr_len);
	  log("Sent from port %d\n", ntohs(my_addr.sin_port));
          block_number++;
        }
      }
    }
  } else if (wflag) {
    block_number = 0;
    while (loopcond) {
      // log("Calling for return packet\n");
      addr_len = sizeof(struct sockaddr_in);
      if ((numbytes = recvfrom(sockfd, recvbuf, MAXBUFLEN - 1, 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
      }
      log("Got packet from %s, port %d\n",
              inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
      log("Packet is %d bytes long\n", numbytes);
      recvbuf[numbytes] = '\0';
      log("Packet contains \"%s\"\n", recvbuf);
      if (strncmp(recvbuf, OPCODE_ACK, 2) == 0) {
        if (recvbuf[2] == (char)(block_number / 10 + 48) &&
            recvbuf[3] == (char)(block_number % 10 + 48)) {
          log("New received ack\n");
          block_number++;
          sendbuf[0] = '\0';
          strcat(sendbuf, OPCODE_DAT);
          sendbuf[2] = (char)(block_number / 10 + 48);
          sendbuf[3] = (char)(block_number % 10 + 48);
	  bufferPos = 4;
	  // Add the data to the packet!
          if (fgets(fileLine, MAXDATALEN, ioFile) != NULL) {
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
          log("send %d bytes to %s\n", numbytes,
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
