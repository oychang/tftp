#include "client.h"
//=============================================================================
int tftp_client(const int PORT, const int rflag, char *file_name,
  char *host_name) {
  int sockfd;
  struct sockaddr_in myaddr;
  struct sockaddr_in serveraddr;
  struct sockaddr_in recvaddr;
  unsigned int addrLen;            // Designates length of IP addresses
  static const int yes = 1;        // Necessary for setting socket options
  static const struct timeval timeout = {.tv_sec = TIMEOUT_SEC, .tv_usec = 0 };
  static const char * mode = "octet";
  char sendbuf[MAXBUFLEN];         // Buffer for sending packets
  char recvbuf[MAXBUFLEN];         // Buffer for receiving packets
  int numbytes = -1;               // Number of bytes being sent
  int rqBufferPos = 0;
  int addBufferPos = 4;
  int blockNumber;                 // Current block # for ack or data
  FILE *ioFile;                    // Local file to read or write from

  // Create a socket and return its integer descriptor
  if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  log("Success in obtaining UDP sockfd %d\n", sockfd);

  // Set socket options: port reusal, send/receive timeouts
  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
     sizeof(int)) == -1) {
    perror("setsockopt");
    log("Continuing without port reuse\n");
  } else {
    log("Successfully set port reuse\n");
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout,
     sizeof(struct timeval)) == -1) {
    perror("setsockopt");
    log("Continuing without receive timeout\n");
  } else {
    log("Successfully set receive timeout to %d seconds\n", TIMEOUT_SEC);
  }
  if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, (void*)&timeout,
     sizeof(struct timeval)) == -1) {
    perror("setsockopt");
    log("Continuing without send timeout\n");
  } else {
    log("Successfully set send timeout to %d seconds\n", TIMEOUT_SEC);
  }

  // Specify values for the structure specifying the server address
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_port = htons(PORT);
  int ptonret = inet_pton(AF_INET, host_name, &serveraddr.sin_addr);
  if (ptonret != 1) {
    if (ptonret == 0) {
      log("ptonret returned 0: not valid family address");
    } else {
      perror("ptonret");
    }
    exit(1);
  }
  memset(&(serveraddr.sin_zero), '\0', 8);

  // Specify values for the structure specifying client's own address
  myaddr.sin_family = AF_INET;
  myaddr.sin_port = htons(0);
  myaddr.sin_addr.s_addr = INADDR_ANY;
  memset(&(myaddr.sin_zero), '\0', 8);

  // Bind to the client's ephemeral port, so packets can be received on it
  if (bind(sockfd, (struct sockaddr *)&myaddr,
      sizeof(struct sockaddr)) == -1) {
    perror("bind");
    exit(1);
  }
  log("Successfully bound to ephemeral port %d!\n", ntohs(myaddr.sin_port));

  // Pack and send the initial read/write request; establish connection
  // If rflag is set, opcode 01; if wflag is set, opcode 02
  if (rflag) {
    if ((ioFile = fopen(file_name, "w")) == NULL) {
      perror("fopen");
      exit(1);
    }
    log("Preparing read request packet!\n");
    memcpy(sendbuf, (char [2]){0, OPCODE_RRQ}, 2*sizeof(char));
    rqBufferPos += 2;
  } else {
    if ((ioFile = fopen(file_name, "r")) == NULL) {
      perror("fopen");
      exit(1);
    }
    log("Preparing write request packet!\n");
    memcpy(sendbuf, (char [2]){0, OPCODE_WRQ}, 2*sizeof(char));
    rqBufferPos += 2;
  }

  // Add the target file name into the request packet's buffer
  strncpy(&sendbuf[rqBufferPos], file_name, MAXBUFLEN-rqBufferPos);
  rqBufferPos += strlen(file_name) + 1;
  // Add the mode into the request packet's buffer
  strncpy(&sendbuf[rqBufferPos], mode, MAXBUFLEN-rqBufferPos);
  rqBufferPos += strlen(mode) + 1;

  // Attempt to send the initial request packet off to the server
  // If successful, print out details of the transmission (size, destination)
  if ((numbytes = sendto(sockfd, sendbuf, rqBufferPos, 0,
       (struct sockaddr *)&serveraddr, sizeof(struct sockaddr))) == -1) {
    perror("sendto");
    exit(1);
  }
  log("Sending %d bytes to %s, server port: %d\n", numbytes,
      inet_ntoa(serveraddr.sin_addr), ntohs(serveraddr.sin_port));
  addrLen = sizeof(struct sockaddr_in);
  getsockname(sockfd, (struct sockaddr *)&myaddr, &addrLen);
  log("Sent %d bytes via client IP %s, client port %d\n", numbytes,
      inet_ntoa(myaddr.sin_addr), ntohs(myaddr.sin_port));

  // Receive at the top of this loop, send at end
  blockNumber = rflag ? 1 : 0;
  while (true) {
    log("Waiting for packet\n");

    do {
      log("listening for packet\n");
      addrLen = sizeof(struct sockaddr);
      numbytes = recvfrom(sockfd, recvbuf, MAXBUFLEN, 0,
        (struct sockaddr *)&recvaddr, &addrLen);
    } while (numbytes == -1);

    log("Got packet from %s, port %d\n",
      inet_ntoa(recvaddr.sin_addr), ntohs(recvaddr.sin_port));
    log("Packet is %d bytes long\n", numbytes);

    // After the first packet, send packets to the server's chosen
    // ephemeral port.
    if (serveraddr.sin_port == htons(PORT)) {
      log("changing their addr port\n");
      serveraddr.sin_port = recvaddr.sin_port;
    }

    recvbuf[numbytes] = '\0';
    int recvBlockNum = (recvbuf[2] << 8) + recvbuf[3];

    // Parse data packet, send response ack packet
    if (rflag && recvbuf[0] == 0 && recvbuf[1] == OPCODE_DAT) {
      // Check if right block number
      if (blockNumber != recvBlockNum) {
        log("Data for block was already received; ignoring packet\n");
        continue;
      }

      log("Received data for block# %d\n", recvBlockNum);

      if (fputs(&recvbuf[4], ioFile) != EOF) {
        log("Successfully wrote newly received data to local file!\n");
      } else {
        // XXX: prepare error packet
        log("write to local file");
        exit(1);
      }

      // Construct an acknowledgement packet and send back
      memcpy(sendbuf, (char [4]){
        0, OPCODE_ACK,
        GET_HOB(blockNumber), GET_LOB(blockNumber)
      }, 4*sizeof(char));

      // Check if done
      if (numbytes < 516) {
        log("Got incomplete data packet so done with transfer after ack\n");
        sendto(sockfd, sendbuf, addBufferPos, 0,
          (struct sockaddr *)&serveraddr, sizeof(struct sockaddr));
        break;
      }

      if ((numbytes = sendto(sockfd, sendbuf, addBufferPos, 0,
        (struct sockaddr *)&serveraddr, sizeof(struct sockaddr))) == -1) {
        // XXX: sloppy, preventable death here
        perror("sendto");
        exit(1);
      }
      blockNumber++;

    // Parse ack packet, send response data packet
    } else if (!rflag && recvbuf[0] == 0 && recvbuf[1] == OPCODE_ACK) {
      if (blockNumber != recvBlockNum) {
        log("Ack for block %d was already received; ignoring packet\n",
          blockNumber);
        continue;
      }

      log("Received ack for block# %d\n", blockNumber);
      blockNumber++;

      memcpy(sendbuf, (char [4]){
        0, OPCODE_DAT,
        GET_HOB(blockNumber), GET_LOB(blockNumber)
      }, 4*sizeof(char));
      addBufferPos = 4;
      // Add the data to the packet
      addBufferPos += fread(&sendbuf[4], sizeof(char), 512, ioFile);

      if ((numbytes = sendto(sockfd, sendbuf, addBufferPos, 0,
        (struct sockaddr *)&serveraddr, sizeof(struct sockaddr))) == -1) {
        // XXX: sloppy, preventable death
        perror("sendto");
        exit(1);
      }

      if (numbytes < 516) {
        log("Sent incomplete data packet so done with transfer\n");
        break;
      }

    // parse error packet
    } else if (recvbuf[0] == 0 && recvbuf[1] == OPCODE_ERR) {
      fprintf(stderr, "err packet, code %d\n", recvBlockNum);
      switch (recvBlockNum) {
      case 0:
        log("Error Code 0: Not defined, see error message (if any)\n");
        log("Error Message: %s\n", &recvbuf[4]);
        break;
      case 1:
        log("Error Code 1: File not found\n");
        break;
      case 2:
        log("Error Code 2: Access violation\n");
        break;
      case 3:
        log("Error Code 3: Disk full or allocation exceeded\n");
        break;
      case 4:
        log("Error Code 4: Illegal TFTP operation\n");
        break;
      case 5:
        log("Error Code 5: Unknown transfer ID\n");
        break;
      case 6:
        log("Error Code 6: File already exists\n");
        break;
      case 7: default:
        log("Error Code 7: No such user\n");
        break;
      }
      // Break out of while loop, ending transfer
      break;
    } else {
      log("Opcode Mismatch... Expecting %s packets; ignoring packet\n",
        rflag ? "data" : "ack");
    }
  }

  if (fclose(ioFile) == EOF) {
    perror("fclose");
    exit(1);
  }

  log("Terminating communication; closing communication channel!\n");
  close(sockfd);
  return EXIT_SUCCESS;
}
//=============================================================================
