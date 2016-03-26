#include "common.h"
#include <pthread.h>

#include <string.h>
#include <getopt.h>


int get_message(int server_fd, struct sockaddr_in *sender_addr);
int send_reply(int server_fd, struct sockaddr_in* dest_addr);
int parse_addresses(char* input, const int input_len, const int server_port, struct sockaddr_in *addresses, const int max_addr_count, int* count);
int generate_default_addresses(struct sockaddr_in *bind_addrs, int* count, const int server_port);
void print_usage(const char *binary_name);

int main(int argc, char* argv[])
{
    int server_fd = 0;
    char* local_ipaddr_list = NULL;
    int server_port = SERVER_PORT;
    int local_addr_count = 0;
    struct sockaddr_in local_addrs[MAX_MULTIHOMING_ADDRESSES];

    memset(local_addrs, sizeof(local_addrs), 0);

    //for getopt:
    // * : after parameter - this parameter has got a mandatory value (e.g. -p 80)
    // * : in the beginning - return ':' if parameter value is missing

    int c = 0;
    while ((c = getopt (argc, argv, ":hr:l:p:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
            break;

            case 'l':
                local_ipaddr_list = optarg;
            break;

            case 'p':
                server_port = atoi(optarg);

            break;

            case ':':
                printf("Missing parameter value for -%c\n", optopt);
            return 1;
            break;

            case '?':
                printf("Unknown option: -%c\n", optopt);
                print_usage(argv[0]);
            return 2;
            break;

            default:
                printf("Unexpected error!\n");
            return 3;
            break;
        }

    }

    if(validate_port_number(server_port)) {
        return 4;
    }

    if(local_ipaddr_list != NULL) {
        if(parse_addresses(local_ipaddr_list, strlen(local_ipaddr_list), server_port, local_addrs, MAX_MULTIHOMING_ADDRESSES, &local_addr_count)) {
            return 5;
        }
    }
    else {
        if(generate_default_addresses(local_addrs, &local_addr_count, server_port)) {
            return 6;
        }
    }

    if((server_fd = socket(ADDR_FAMILY, SOCK_TYPE, PROTO)) == -1) {
        perror("socket");
        return 7;
    }

    //enable some notifications
    if(enable_notifications(server_fd) != 0) {
        printf("Error enabling notifications\n");
        return 8;
    }


    if(sctp_bindx(server_fd, (struct sockaddr*)&local_addrs, local_addr_count, SCTP_BINDX_ADD_ADDR) == -1) {
        perror("bind");
        return 9;
    }

    if(listen(server_fd, SERVER_LISTEN_QUEUE_SIZE) != 0) {
        perror("listen");
        return 10;
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

    return 11;
}

int get_message(int server_fd, struct sockaddr_in* sender_addr)
{
    char payload[1024];
    int buffer_len = sizeof(payload) - 1;
    memset(&payload, 0, sizeof(payload));

    int sender_addr_size = sizeof(struct sockaddr_in);

    struct sctp_sndrcvinfo snd_rcv_info;
    memset(&snd_rcv_info, 0, sizeof(snd_rcv_info));

    int msg_flags = 0;

    while(1) {
        int recv_size = 0;

        if((recv_size = sctp_recvmsg(server_fd, &payload, buffer_len, (struct sockaddr*)sender_addr, &sender_addr_size, &snd_rcv_info, &msg_flags)) == -1) {
            printf("recvmsg() error\n");
            return 1;
        }

        if(msg_flags & MSG_NOTIFICATION) {
            if(!(msg_flags & MSG_EOR)) {
                printf("Notification received, but the buffer is not big enough.\n");
                continue;
            }

            handle_notification((union sctp_notification*)payload, recv_size, server_fd);
        }
        else if(msg_flags & MSG_EOR) {
            printf("%s\n", payload);
            break;
        }
        else {
            printf("%s", payload); //if EOR flag is not set, the buffer is not big enough for the whole message
        }
    }

    return 0;
}

int send_reply(int server_fd, struct sockaddr_in* dest_addr)

{
    char buf[8];
    memset(buf, 0, sizeof(buf));
    strncpy(buf, "OK", sizeof(buf)-1);

    if(sctp_sendmsg(server_fd, buf, strlen(buf), (struct sockaddr*)dest_addr, sizeof(struct sockaddr_in), 0, 0, 0, 0, 0) == -1) {
        printf("sendmsg() error\n");
        return 1;
    }

    return 0;
}

int generate_default_addresses(struct sockaddr_in* bind_addrs, int* count, const int server_port)
{
    memset(bind_addrs, 0, sizeof(struct sockaddr_in));
    bind_addrs->sin_family = ADDR_FAMILY;
    bind_addrs->sin_port = htons(server_port);
    bind_addrs->sin_addr.s_addr = INADDR_ANY;

    *count = 1;

    return 0;
}


void print_usage(const char* binary_name)
{
    printf("SCTP server usage:\n"
           "%s -l <local IP addresses> -p <port> \n"
           "Where:\n"
           "- local IP addresses - Semicolon separated list of local IP addresses to bind to, in single quotes, no tabs/spaces!\n"
           "- port - Server's port to connect to\n",
           binary_name);
}
