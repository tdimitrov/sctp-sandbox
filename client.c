#include "common.h"

int send_message(int server_fd, struct sockaddr_in *dest_addr, char* payload, int payload_len);
int get_reply(int server_fd);

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

    //enable some notifications
    if(enable_notifications(client_fd) != 0) {
        printf("Error enabling notifications.\n");
        return 4;
    }

    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(struct sockaddr_in));
    peer_addr.sin_family = ADDR_FAMILY;
    peer_addr.sin_port = htons(server_port);
    if(inet_pton(AF_INET, server_ip, &(peer_addr.sin_addr)) != 1) {
        printf("Error converting IP address (%s) to sockaddr_in structure\n", server_ip);
        return 5;
    }

    printf("Connecting...\n");

    if(connect(client_fd, (struct sockaddr*)&peer_addr, sizeof(peer_addr)) == -1) {
        perror("connect");
        return 6;
    }

    printf("OK\n");

    char buf[1024];
    for(int i = 0; i < CLIENT_SEND_COUNT; ++i) {
        printf("Sending message %d of %d. Result: ", i+1, CLIENT_SEND_COUNT);

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "DATA %d", i);

        if(send_message(client_fd, &peer_addr, buf, strlen(buf))) {
            return 1;
        }

        if(get_reply(client_fd)) {
            return 2;
        }

        memset(buf, 0, sizeof(buf));
    }

    printf("Closing...\n");
    if(close(client_fd) == -1) {
        perror("close");
        return 8;
    }

    return 0;
}

int send_message(int server_fd, struct sockaddr_in *dest_addr, char* payload, int payload_len)
{
    struct iovec io_buf;
    io_buf.iov_base = payload;
    io_buf.iov_len = payload_len;

    struct msghdr msg;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_iov = &io_buf;
    msg.msg_iovlen = 1;
    msg.msg_name = dest_addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);

    if(sendmsg(server_fd, &msg, 0) == -1) {
        printf("sendmsg() error\n");
        return 1;
    }

    return 0;
}

int get_reply(int server_fd)
{
    struct sockaddr_in dest_addr;

    char payload[1024];
    int buffer_len = sizeof(payload) - 1;
    memset(&payload, 0, sizeof(payload));

    struct iovec io_buf;
    memset(&payload, 0, sizeof(payload));
    io_buf.iov_base = payload;
    io_buf.iov_len = buffer_len;

    struct msghdr msg;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_iov = &io_buf;
    msg.msg_iovlen = 1;
    msg.msg_name = &dest_addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);

    while(1) {
        int recv_size = 0;
        if((recv_size = recvmsg(server_fd, &msg, 0)) == -1) {
            printf("recvmsg() error\n");
            return 1;
        }

        if(msg.msg_flags & MSG_NOTIFICATION) {
            if(!(msg.msg_flags & MSG_EOR)) {
                printf("Notification received, but the buffer is not big enough.\n");
                continue;
            }

            handle_notification((union sctp_notification*)payload, recv_size, server_fd);
        }
        else if(msg.msg_flags & MSG_EOR) {
            printf("%s\n", payload);
            break;
        }
        else {
            printf("%s", payload); //if EOR flag is not set, the buffer is not big enough for the whole message
        }
    }

    return 0;
}
