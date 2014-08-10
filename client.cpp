#include "common.h"

int initmsg(int socket)
{
      struct sctp_initmsg initmsg;
      memset(&initmsg, 0, sizeof(initmsg));
      socklen_t initmsg_len = sizeof(struct sctp_initmsg);
      if(getsockopt(socket, SOL_SCTP, SCTP_INITMSG, &initmsg, &initmsg_len) == -1) {
            perror("getsockopt sctp_initmsg");
            return 1;
      }

      printf("sctp_initmsg.max_attempts: %d\n", initmsg.sinit_max_attempts);
      printf("sctp_initmsg.max_inittimeo: %d\n", initmsg.sinit_max_init_timeo);
      printf("sctp_initmsg.max_instreams: %d\n", initmsg.sinit_max_instreams);
      printf("sctp_initmsg.num_ostreams: %d\n", initmsg.sinit_num_ostreams);


      initmsg.sinit_num_ostreams = 8;
      initmsg.sinit_max_instreams = 8;
      if(setsockopt(socket, SOL_SCTP, SCTP_INITMSG, &initmsg, sizeof(initmsg)) == -1) {
            perror("setsockopt sctp_initmsg");
            return 2;
      }

      return 0;
}


int main()
{
      int client_fd = 0;

      if((client_fd = socket(ADDR_FAMILY, SOCK_TYPE, PROTO)) == -1) {
            perror("socket");
            return 1;
      }

      struct sockaddr_in peer_addr;
      memset(&peer_addr, 0, sizeof(struct sockaddr_in));
      peer_addr.sin_family = ADDR_FAMILY;
      peer_addr.sin_port = htons(SERVER_PORT);
      if(inet_pton(AF_INET, SERVER_IP, &(peer_addr.sin_addr)) != 1) {
            printf("Error converting IP address\n");
            return 2;
      }

      printf("Connecting...\n");

      if(connect(client_fd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) == -1) {
            perror("connect");
            return 2;
      }

      printf("OK\n");

      char buf[1024];
      for(int i = 0; i < CLIENT_SEND_COUNT; ++i) {
            printf("Sending message %d of %d. Result: ", i+1, CLIENT_SEND_COUNT);

            memset(buf, 0, sizeof(buf));
            snprintf(buf, sizeof(buf)-1, "DATA %d", i);

            if(send(client_fd, &buf, strlen(buf), 0) == -1) {
                  perror("send");
                  return 3;
            }

            memset(buf, 0, sizeof(buf));

            if(recv(client_fd, &buf, sizeof(buf), 0) == -1) {
                  perror("recv");
                  return 4;
            }

            printf("%s\n", buf);
      }

      printf("Closing...\n");
      if(close(client_fd) == -1) {
            perror("close");
            return 5;
      }

      return 0;
}
