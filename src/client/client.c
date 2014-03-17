#include "client.h"
//=============================================================================
int tftp_client(int port, int rflag, char *file_name, char *host_name) {

  int default_sockfd;
  int current_sockfd;
  struct sockaddr_in their_addr;   // Structure to hold server IP address
  struct sockaddr_in my_addr;      // Structure to hold client IP address
  struct sockaddr_in from_addr;
  unsigned int addr_len;           // Designates length of IP addresses
  struct hostent *he;              // Pointer to a host table entry
  static const int yes = 1;        // Necessary for setting socket options
  static const struct timeval timeout = {.tv_sec = TIMEOUT_SEC, .tv_usec = 0 };
  static const char * mode = "octet";
  char sendbuf[MAXBUFLEN];         // Buffer for sending packets
  char recvbuf[MAXBUFLEN];         // Buffer for receiving packets
  int numbytes = -1;               // Number of bytes being sent
  int rqBufferPos = 0;
  int addBufferPos = 4;
  int block_number;                // Current block # for ack or data
  FILE *ioFile;                    // Local file to read or write from

  // Get IP address from specified host name
  if ((he = gethostbyname(host_name)) == NULL) {
    perror("gethostbyname");
    exit(1);
  }

  // Create a socket and return its integer descriptor
  if ((default_sockfd = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
    perror("socket");
    exit(1);
  }
  log("Success in obtaining UDP sockfd %d\n", default_sockfd);

  // Set socket options: port reusal, send/receive timeouts
  if (setsockopt(default_sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
     sizeof(int)) == -1) {
    perror("setsockopt");
    log("Continuing without port reuse\n");
  } else {
    log("Successfully set port reuse\n");
  }
  if (setsockopt(default_sockfd, SOL_SOCKET, SO_RCVTIMEO, (void*)&timeout,
     sizeof(struct timeval)) == -1) {
    perror("setsockopt");
    log("Continuing without receive timeout\n");
  } else {
    log("Successfully set receive timeout to %d seconds\n", TIMEOUT_SEC);
  }
  if (setsockopt(default_sockfd, SOL_SOCKET, SO_SNDTIMEO, (void*)&timeout,
     sizeof(struct timeval)) == -1) {
    perror("setsockopt");
    log("Continuing without send timeout\n");
  } else {
    log("Successfully set send timeout to %d seconds\n", TIMEOUT_SEC);
  }

  // Specify values for the structure specifying the server address
  their_addr.sin_family = AF_INET;
  their_addr.sin_port = htons(port);
  their_addr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(&(their_addr.sin_zero), '\0', 8);

  // Specify values for the structure specifying client's own address
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(0);
  my_addr.sin_addr.s_addr = INADDR_ANY;
  memset(&(my_addr.sin_zero), '\0', 8);

  // Bind to the client's ephemeral port, so packets can be received on it
  if (bind(default_sockfd, (struct sockaddr *)&my_addr,
      sizeof(struct sockaddr)) == -1) {
    perror("bind");
    exit(1);
  }
  log("Successfully bound to ephemeral port %d!\n", ntohs(my_addr.sin_port));

  current_sockfd = default_sockfd;

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
  if ((numbytes = sendto(default_sockfd, sendbuf, rqBufferPos, 0,
       (struct sockaddr *)&their_addr, sizeof(struct sockaddr))) == -1) {
    perror("sendto");
    exit(1);
  }
  log("Sending %d bytes to %s, server default port: %d\n", numbytes,
      inet_ntoa(their_addr.sin_addr), ntohs(their_addr.sin_port));
  addr_len = sizeof(struct sockaddr_in);
  getsockname(default_sockfd, (struct sockaddr *)&my_addr, &addr_len);
  log("Sent %d bytes via client IP %s, client port %d\n", numbytes,
      inet_ntoa(my_addr.sin_addr), ntohs(my_addr.sin_port));

  // Receive at the top of this loop, send at end
  block_number = rflag ? 1 : 0;
  while (true) {
    log("Listening on sockfd: %d\n", current_sockfd);

    addr_len = sizeof(struct sockaddr);
    numbytes = recvfrom(current_sockfd, recvbuf, MAXBUFLEN - 1, 0,
      (struct sockaddr *)&from_addr, &addr_len);
    while (numbytes == -1) {
      log("Failed to receive packet from server; retrying.\n");
      addr_len = sizeof(struct sockaddr);
      numbytes = recvfrom(current_sockfd, recvbuf, MAXBUFLEN - 1, 0,
        (struct sockaddr *)&from_addr, &addr_len);
    }

    log("Got packet from %s, port %d\n",
      inet_ntoa(from_addr.sin_addr), ntohs(from_addr.sin_port));
    log("Packet is %d bytes long\n", numbytes);

    // After the first packet, send packets to the server's chosen
    // ephemeral port.
    if (their_addr.sin_port == htons(port)) {
      log("changing their addr port\n");
      their_addr.sin_port = from_addr.sin_port;
    }

    recvbuf[numbytes] = '\0';
    int recvBlockNum = (recvbuf[2] << 8) + recvbuf[3];
    log("Received data with block# %d\n", recvBlockNum);

    // Parse data packet, send response ack packet
    if (rflag && recvbuf[0] == 0 && recvbuf[1] == OPCODE_DAT) {
      // Check if right block number
      if (block_number != recvBlockNum) {
        log("Data for block was already received; ignoring packet\n");
        continue;
      }

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
        GET_HOB(block_number), GET_LOB(block_number)
      }, 4*sizeof(char));

      // Check if done
      if (numbytes < 516) {
        log("Got incomplete data packet so done with transfer after ack\n");
        sendto(current_sockfd, sendbuf, addBufferPos, 0,
          (struct sockaddr *)&their_addr, sizeof(struct sockaddr));
        break;
      }

      if ((numbytes = sendto(current_sockfd, sendbuf, addBufferPos, 0,
        (struct sockaddr *)&their_addr, sizeof(struct sockaddr))) == -1) {
        // XXX: sloppy, preventable death here
        perror("sendto");
        exit(1);
      }
      block_number++;

    // Parse ack packet, send response data packet
    } else if (!rflag && recvbuf[0] == 0 && recvbuf[1] == OPCODE_ACK) {
      if (block_number != recvBlockNum) {
        log("Ack for block %d was already received; ignoring packet\n",
          block_number);
        continue;
      }

      log("Received ack for block# %d\n", block_number);
      block_number++;

      memcpy(sendbuf, (char [4]){
        0, OPCODE_DAT,
        GET_HOB(block_number), GET_LOB(block_number)
      }, 4*sizeof(char));
      addBufferPos = 4;
      // Add the data to the packet
      addBufferPos += fread(&sendbuf[4], sizeof(char), 512, ioFile);

      if ((numbytes = sendto(current_sockfd, sendbuf, addBufferPos, 0,
        (struct sockaddr *)&their_addr, sizeof(struct sockaddr))) == -1) {
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
  close(current_sockfd);
  return EXIT_SUCCESS;
}
//=============================================================================
