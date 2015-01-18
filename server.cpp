#include "common.h"
#include <thread>

//TODO: install libsctp-dev
//man 7 ip - ip address structs

void handle_connection(int conn_fd, struct sockaddr addr);

int main(int argc, char* argv[])
{
    int server_fd = 0;
    int server_port = SERVER_PORT;

    if(argc == 2) {
        server_port = atoi(argv[1]);
        if(server_port < 1 || server_port > 65535) {
            printf("Invalid port number (%d). Valid values are between 1 and 65535.\n", server_port);
            return 1;
        }
    }
    else if(argc != 1) { // argc can be two (port number passed) or one (no params passed)
        printf("Usage: %s [PORT NUMBER TO BIND TO]\n", argv[0]);
        return 2;
    }

    if((server_fd = socket(ADDR_FAMILY, SOCK_TYPE, PROTO)) == -1) {
        perror("socket");
        return 3;
    }

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(struct sockaddr_in));
    bind_addr.sin_family = ADDR_FAMILY;
    bind_addr.sin_port = htons(server_port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) == -1) {
        perror("bind");
        return 4;
    }

    if(listen(server_fd, SERVER_LISTEN_QUEUE_SIZE) != 0) {
        perror("listen");
        return 5;
    }

    printf("Listening on port %d\n", server_port);

    while(1) {
        struct sockaddr client_addr;
        memset(&client_addr, 0, sizeof(client_addr));
        unsigned int client_addr_len = sizeof(client_addr);
        int conn_fd = 0;

        if((conn_fd = accept(server_fd, &client_addr, &client_addr_len)) == -1) {
            perror("accept");
            return 6;
        }

        std::thread t(std::bind(&handle_connection, conn_fd, client_addr));
        t.detach();
    }

    return 0;
}

void handle_connection(int socket, struct sockaddr addr)
{
    struct sockaddr_in* p_addr = (struct sockaddr_in*) &addr;	//cast the addr to sockaddr_in
    char client_ipaddr[128];
    memset(client_ipaddr, 0, sizeof(client_ipaddr));

    if(inet_ntop(AF_INET, &p_addr->sin_addr, client_ipaddr, sizeof(client_ipaddr)) == NULL) {
        perror("inet_ntop");
        return;
    }

    printf("Got connection from %s\n", client_ipaddr);

    while(1) {
        char buf[1024];
        int recv_len = 0;

        if((recv_len = recv(socket, &buf, sizeof(buf), 0)) == -1) {
            perror("recv");
            return;
        }

        if(recv_len == 0) {
            printf("Connection from %s closed by remote peer.\n", client_ipaddr);
            if(close(socket) == -1) {
                perror("close");
            }
            return;
        }

        strncpy(buf, "OK", sizeof(buf)-1);

        if(send(socket, &buf, strlen(buf), 0) == -1) {
            perror("send");
            return;
        }
    }
}
