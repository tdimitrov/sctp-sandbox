#include "common.h"
#include <pthread.h>

int get_message(int server_fd, struct sockaddr_in *sender_addr);
int send_reply(int server_fd, struct sockaddr_in* dest_addr);

int enable_ancillary_data(int fd)
{
    int boolean_as_int = 1;
    if(setsockopt(fd, PROTO, SCTP_RECVRCVINFO, &boolean_as_int, sizeof(boolean_as_int))) {
        printf("Error setting SCTP_RECVRCVINFO\n");
        return 1;
    }

    if(setsockopt(fd, PROTO, SCTP_RECVNXTINFO, &boolean_as_int, sizeof(boolean_as_int))) {
        printf("Error setting SCTP_RECVNXTINFO\n");
        return 2;
    }

    return 0;
}

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

    if(enable_ancillary_data(server_fd)) {
        return 4;
    }

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(struct sockaddr_in));
    bind_addr.sin_family = ADDR_FAMILY;
    bind_addr.sin_port = htons(server_port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_fd, (struct sockaddr*)&bind_addr, sizeof(bind_addr)) == -1) {
        perror("bind");
        return 5;
    }

    if(listen(server_fd, SERVER_LISTEN_QUEUE_SIZE) != 0) {
        perror("listen");
        return 6;
    }

    printf("Listening on port %d\n", server_port);

    struct sockaddr_in addr_buf;

    while(1) {
        memset(&addr_buf, 0, sizeof(addr_buf));

        if(get_message(server_fd, &addr_buf)) {
            break;
        }

        if(send_reply(server_fd, &addr_buf)) {
            break;
        }
    }

    return 7;
}

void handle_ancillary_data(struct msghdr* msg)
{
    struct cmsghdr *cmsgptr;
    for(cmsgptr = CMSG_FIRSTHDR(msg); cmsgptr != NULL; cmsgptr = CMSG_NXTHDR(msg, cmsgptr)) {
        if (cmsgptr->cmsg_len == 0) {
            printf("Error handling ancillary data!\n");
            break;
        }
        if(cmsgptr->cmsg_level == PROTO && cmsgptr->cmsg_type == SCTP_RCVINFO) {
            u_char *ptr = CMSG_DATA(cmsgptr);
            struct sctp_rcvinfo* ri = (struct sctp_rcvinfo*)ptr;

            printf("SCTP_RCVINFO: sid: %u, ssn: %u, flags: %u, ppid: %u, tsn: %u, cumtsn: %u, context: %u, assoc_id: %d\n",
                   ri->rcv_sid, ri->rcv_ssn, ri->rcv_flags, ri->rcv_ppid, ri->rcv_tsn, ri->rcv_cumtsn, ri->rcv_context, ri->rcv_assoc_id);
        }
        else if(cmsgptr->cmsg_level == PROTO && cmsgptr->cmsg_type == SCTP_NXTINFO) {
            u_char *ptr = CMSG_DATA(cmsgptr);
            struct sctp_nxtinfo* nri = (struct sctp_nxtinfo*)ptr;

            printf("SCTP_NXTINFO: sid: %u, flags: %u, ppid: %u, length: %u, assoc_id: %d\n",
                   nri->nxt_sid, nri->nxt_flags, nri->nxt_ppid, nri->nxt_length, nri->nxt_assoc_id);
        }
    }
}

int get_message(int server_fd, struct sockaddr_in* sender_addr)
{
    char payload[1024];
    int buffer_len = sizeof(payload) - 1;
    memset(&payload, 0, sizeof(payload));

    struct iovec io_buf;
    memset(&payload, 0, sizeof(payload));
    io_buf.iov_base = payload;
    io_buf.iov_len = buffer_len;

    char* ancillary_data = NULL;
    int ancillary_data_len = CMSG_SPACE(sizeof(struct sctp_rcvinfo)) + CMSG_SPACE(sizeof(struct sctp_nxtinfo));
    if((ancillary_data = malloc(ancillary_data_len)) == NULL) {
        printf("Error allocating memory\n");
        return 1;
    }
    memset(ancillary_data, 0, ancillary_data_len);

    struct msghdr msg;
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_iov = &io_buf;
    msg.msg_iovlen = 1;
    msg.msg_name = sender_addr;
    msg.msg_namelen = sizeof(struct sockaddr_in);
    msg.msg_control = ancillary_data;
    msg.msg_controllen = ancillary_data_len;

    while(1) {
        int recv_size = 0;
        if((recv_size = recvmsg(server_fd, &msg, 0)) == -1) {
            printf("recvmsg() error\n");
            return 2;
        }

        handle_ancillary_data(&msg);

        if(msg.msg_flags & MSG_EOR) {
            printf("%s\n", payload);
            break;
        }
        else {
            printf("%s", payload); //if EOR flag is not set, the buffer is not big enough for the whole message
        }
    }

    free(ancillary_data);

    return 0;
}

int send_reply(int server_fd, struct sockaddr_in* dest_addr)

{
    char buf[8];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, "OK", sizeof(buf)-1);

    struct iovec io_buf;
    io_buf.iov_base = buf;
    io_buf.iov_len = sizeof(buf);

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
