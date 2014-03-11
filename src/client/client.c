#include "client.h"

int tftp_client(int port, int rflag, char *file_name, char *host_name) {

  int default_sockfd;
  int current_sockfd;
  struct sockaddr_in their_addr;
  struct sockaddr_in ephemeral;
  struct hostent *he;
  struct sockaddr_in my_addr;
  unsigned int addr_len;
  char sendbuf[MAXBUFLEN];
  char recvbuf[MAXBUFLEN];
  char mode[MAXMODELEN] = "octet";
  int numbytes;
  int wflag = (rflag ? 0 : 1);
  int rqBufferPos = 0;
  int addBufferPos = 4;
  int loopcond = 1;
  int first_packet;
  int block_number;
  FILE *ioFile;
  char fileLine[MAXDATALEN];

  if ((he = gethostbyname(host_name)) == NULL) {
    perror("gethostbyname");
    exit(1);
  }

  if ((default_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  their_addr.sin_family = AF_INET;
  their_addr.sin_port = htons(port);
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(&(their_addr.sin_zero), '\0', 8);
  if (bind(default_sockfd, (struct sockaddr *)&their_addr, 
      sizeof(struct sockaddr)) == -1) {
    perror("bind");
    exit(1);
  }
  
  current_sockfd = default_sockfd;

  // Pack and send the initial read/write request; establish connection
  // if rflag is set, opcode 01; if wflag is set, opcode 02
  if (rflag) {
    if ((ioFile = fopen(file_name, "w")) == NULL) {
      perror("opening local file for writing");
      exit(1);
    }
    log("Preparing read request packet!\n");
    memcpy(sendbuf, (char [2]){0, OPCODE_RRQ}, 2*sizeof(char));
    rqBufferPos += 2;
  } else {
    if ((ioFile = fopen(file_name, "r")) == NULL) {
      perror("opening local file for reading");
      exit(1);
    }
    log("Preparing write request packet!\n");
    memcpy(sendbuf, (char [2]){0, OPCODE_WRQ}, 2*sizeof(char));
    rqBufferPos += 2;
  }

  // Add the target file name into the buffer
  strcpy(&sendbuf[rqBufferPos], file_name);
  rqBufferPos += strlen(file_name);
  sendbuf[rqBufferPos] = '\0';
  rqBufferPos++;

  strcpy(&sendbuf[rqBufferPos], mode);
  rqBufferPos += strlen(mode);
  sendbuf[rqBufferPos] = '\0';
  rqBufferPos++;

  if ((numbytes = sendto(current_sockfd, sendbuf, rqBufferPos, 0,
			 (struct sockaddr *)&their_addr,
			 sizeof(struct sockaddr))) == -1) {
    perror("sendto");
    exit(1);
  }
  log("Preparing to send %d bytes to %s\n", numbytes,
	 inet_ntoa(their_addr.sin_addr));
  addr_len = sizeof(struct sockaddr_in);
  getsockname(current_sockfd, (struct sockaddr *)&my_addr, &addr_len);
  log("Sent %d bytes from port %d\n", numbytes, ntohs(my_addr.sin_port));

  //  printf("Sleeping for 5 seconds\n");
  //  sleep(5);

  if (rflag) {
    first_packet = 1;
    block_number = 1;
    while (loopcond) {
      log("Calling for return packet from server...\n");
      addr_len = sizeof(struct sockaddr_in);
      if ((numbytes = recvfrom(current_sockfd, recvbuf, MAXBUFLEN - 1, 0,
	  (struct sockaddr *)&their_addr, &addr_len)) == -1) {
	perror("recvfrom");
        exit(1);
      }

      if (first_packet) {
	first_packet = 0;
	if ((current_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
	  perror("socket");
	  exit(1);
	}
	ephemeral.sin_family = AF_INET;
	ephemeral.sin_port = htons(port);
	ephemeral.sin_addr = *((struct in_addr *)he->h_addr);
	memset(&(ephemeral.sin_zero), '\0', 8);
	if (bind(current_sockfd, (struct sockaddr *)&ephemeral, 
		 sizeof(struct sockaddr)) == -1) {
	  perror("bind");
	  exit(1);
	}
      }

      log("Got packet from %s, port %d\n",
             inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
      log("Packet is %d bytes long\n", numbytes);
      recvbuf[numbytes] = '\0';
      if (numbytes < MAXBUFLEN) {
	loopcond = 0;
      }
      if (recvbuf[0] == 0 && recvbuf[1] == OPCODE_DAT) {
        if ((recvbuf[2] == block_number / 10) &&
	    (recvbuf[3] == block_number % 10)) {
          log("New received data (block %d): %s\n", block_number, &recvbuf[4]);
	  // Have to put the new received data into local file
	  if(fputs(&recvbuf[4], ioFile) != EOF) {
            log("Successfully wrote newly received data to local file!\n");
          } else {
            log("write to local file");
            exit(1);
          }
          // Construct an acknowledgement packet and send back
          sendbuf[0] = '\0';
          memcpy(sendbuf, (char [4]){0, 4, block_number / 10, 
		 block_number % 10}, 4*sizeof(char));
	  log("Preparing to send %d bytes to %s\n", addBufferPos,
	      inet_ntoa(their_addr.sin_addr));
	  if ((numbytes = sendto(current_sockfd, sendbuf, addBufferPos, 0,
	      (struct sockaddr *)&their_addr,
              sizeof(struct sockaddr))) == -1) {
            perror("sendto");
            exit(1);
	  }
	  addr_len = sizeof(struct sockaddr_in);
	  getsockname(current_sockfd, (struct sockaddr *)&my_addr, &addr_len);
	  log("Sent %d bytes (ACK for block %d) from port %d\n", numbytes, 
	      block_number, ntohs(my_addr.sin_port));
          block_number++;
        }
      }
    }
  } else if (wflag) {
    first_packet = 1;
    block_number = 0;
    while (loopcond) {
      log("Calling for return packet from server...\n");
      addr_len = sizeof(struct sockaddr_in);
      if ((numbytes = recvfrom(current_sockfd, recvbuf, MAXBUFLEN - 1, 0,
        (struct sockaddr *)&their_addr, &addr_len)) == -1) {
        perror("recvfrom");
        exit(1);
      }

      if (first_packet) {
	first_packet = 0;
	if ((current_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
	  perror("socket");
	  exit(1);
	}
	ephemeral.sin_family = AF_INET;
	ephemeral.sin_port = htons(port);
	ephemeral.sin_addr = *((struct in_addr *)he->h_addr);
	memset(&(ephemeral.sin_zero), '\0', 8);
	if (bind(current_sockfd, (struct sockaddr *)&ephemeral, 
		 sizeof(struct sockaddr)) == -1) {
	  perror("bind");
	  exit(1);
	}
      }

      log("Got packet from %s, port %d\n",
              inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
      log("Packet is %d bytes long\n", numbytes);
      recvbuf[numbytes] = '\0';
      if (recvbuf[0] == 0 && recvbuf[1] == OPCODE_ACK) {
        if (recvbuf[2] == block_number / 10 &&
            recvbuf[3] == block_number % 10) {
          log("New received ack (block %d)\n", block_number);
          block_number++;
          sendbuf[0] = '\0';
          memcpy(sendbuf, (char [4]){0, OPCODE_DAT, block_number / 10, 
		block_number % 10}, 4*sizeof(char));
	  sendbuf[addBufferPos] = '\0';
	  // Add the data to the packet!
          if (fgets(fileLine, MAXDATALEN, ioFile) != NULL) {
	    fileLine[strlen(fileLine) -1] = '\0';
	    addBufferPos += strlen(fileLine);
            strcat(&sendbuf[4], fileLine);
	  } else {
	    log("Reached end of local data; preparing final data packet!\n");
	  }
	  log("Data to be sent (block %d): %s\n", block_number, &sendbuf[4]);
	  log("Preparing to send %d bytes to %s\n", addBufferPos, 
	      inet_ntoa(their_addr.sin_addr));
	  if ((numbytes = sendto(current_sockfd, sendbuf, addBufferPos, 0,
	      (struct sockaddr *)&their_addr,
	      sizeof(struct sockaddr))) == -1) {
	    perror("sendto");
	    exit(1);
	  }
	  addr_len = sizeof(struct sockaddr_in);
	  getsockname(current_sockfd, (struct sockaddr *)&my_addr, &addr_len);
	  log("Sent %d bytes (data for block %d) from port %d\n", numbytes, 
	      block_number, ntohs(my_addr.sin_port));
	  addBufferPos = 4; // Reset buffer position to 2(opcode) + 2(block)
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
  log("Terminating communication; closing communication channel!\n");
  close(current_sockfd);
  return 0;
}
