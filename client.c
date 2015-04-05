#include "common.h"

int main(int argc, char* argv[])
{
    int client_fd = 0;
    const char* server_ip = NULL;
    int server_port = SERVER_PORT;

    if(argc == 3) {     //user provided IP and PORT
        server_ip = argv[1];
        server_port = atoi(argv[2]);
        if(server_port < 1 || server_port > 65535) {
            printf("Invalid port number (%d). Valid values are between 1 and 65535.\n", server_port);
            return 1;
        }
    }
    else if(argc == 2) {    //user provided only IP
        server_ip = argv[1];
    }
    else {    //user provided something wrong
        printf("Usage: %s [SERVER IP ADDRESS] [SERVER PORT]\n", argv[0]);
        return 2;
    }

    if((client_fd = socket(ADDR_FAMILY, SOCK_TYPE, PROTO)) == -1) {
        perror("socket");
        return 3;
    }

    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(struct sockaddr_in));
    peer_addr.sin_family = ADDR_FAMILY;
    peer_addr.sin_port = htons(server_port);
    if(inet_pton(AF_INET, server_ip, &(peer_addr.sin_addr)) != 1) {
        printf("Error converting IP address (%s) to sockaddr_in structure\n", server_ip);
        return 4;
    }

    printf("Connecting...\n");

    if(connect(client_fd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) == -1) {
        perror("connect");
        return 5;
    }

    printf("OK\n");

    char buf[1024];
    for(int i = 0; i < CLIENT_SEND_COUNT; ++i) {
        printf("Sending message %d of %d. Result: ", i+1, CLIENT_SEND_COUNT);

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "DATA %d", i);

        if(send(client_fd, &buf, strlen(buf), 0) == -1) {
            perror("send");
            return 6;
        }

        memset(buf, 0, sizeof(buf));

        if(recv(client_fd, &buf, sizeof(buf), 0) == -1) {
            perror("recv");
            return 7;
        }

        printf("%s\n", buf);
    }

    printf("Closing...\n");
    if(close(client_fd) == -1) {
        perror("close");
        return 8;
    }

    return 0;
}
