#include <stdio.h>
#include <string.h>
#include <unistd.h>		//for close()
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>	//for inet_ntop()


const int SERVER_PORT = 4444;
const int ADDR_FAMILY = AF_INET;
const int SOCK_TYPE = SOCK_SEQPACKET;
const int PROTO = IPPROTO_SCTP;
const int CLIENT_SEND_COUNT = 5;
const int SERVER_LISTEN_QUEUE_SIZE = 10;

