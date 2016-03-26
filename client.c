#include "common.h"
#include <getopt.h>


int send_message(int server_fd, struct sockaddr_in *dest_addr, char* payload, int payload_len);
int get_reply(int server_fd);
void print_usage(const char *binary_name);

int main(int argc, char* argv[])
{
    int client_fd = 0;
    char* local_ipaddr_list = NULL;
    char* remote_ipaddr_list = NULL;
    int server_port = SERVER_PORT;
    struct sockaddr_in local_addrs[MAX_MULTIHOMING_ADDRESSES];
    struct sockaddr_in remote_addrs[MAX_MULTIHOMING_ADDRESSES];
    int remote_addr_count = 0;
    int local_addr_count = 0;

     memset(local_addrs, sizeof(local_addrs), 0);
     memset(remote_addrs, sizeof(remote_addrs), 0);

    //for getopt:
    // * : after parameter - this parameter has got a mandatory value (e.g. -p 80)
    // * : in the beginning - return ':' if parameter value is missing

    int c = 0;
    while ((c = getopt (argc, argv, ":hr:l:p:")) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
            break;

            case 'r':
                remote_ipaddr_list = optarg;
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

    if(remote_ipaddr_list == NULL) {
        printf("Provide at least one remote IP addresses with -r\n");
        return 5;
    }

    if(parse_addresses(remote_ipaddr_list, strlen(remote_ipaddr_list), server_port, remote_addrs, MAX_MULTIHOMING_ADDRESSES, &remote_addr_count)) {
        return 6;
    }

    if(local_ipaddr_list != NULL) {
        if(parse_addresses(local_ipaddr_list, strlen(local_ipaddr_list), 0, local_addrs, MAX_MULTIHOMING_ADDRESSES, &local_addr_count)) {
            return 7;
        }
    }

    if((client_fd = socket(ADDR_FAMILY, SOCK_TYPE, PROTO)) == -1) {
        perror("socket");
        return 8;
    }

    if(local_ipaddr_list != NULL) {
        if(sctp_bindx(client_fd, (struct sockaddr*)&local_addrs, local_addr_count, SCTP_BINDX_ADD_ADDR) == -1) {
            perror("bind");
            return 9;
        }
    }

    //enable some notifications
    if(enable_notifications(client_fd) != 0) {
        printf("Error enabling notifications.\n");
        return 10;
    }

    printf("Connecting...\n");

    sctp_assoc_t assoc_id = 0;
    if(sctp_connectx(client_fd, (struct sockaddr*)&remote_addrs, remote_addr_count, &assoc_id) == -1) {
        perror("sctp_connectx");
        return 11;
    }

    printf("OK. Association id is %d\n", assoc_id);

    char buf[1024];
    for(int i = 0; i < CLIENT_SEND_COUNT; ++i) {
        printf("Sending message %d of %d. Result: ", i+1, CLIENT_SEND_COUNT);

        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf)-1, "DATA %d", i);

        if(send_message(client_fd, &remote_addrs[0], buf, strlen(buf))) {
            return 12;
        }

        if(get_reply(client_fd)) {
            return 13;
        }

        memset(buf, 0, sizeof(buf));
    }

    printf("Closing...\n");
    if(close(client_fd) == -1) {
        perror("close");
        return 14;
    }

    return 0;
}

int send_message(int server_fd, struct sockaddr_in *dest_addr, char* payload, int payload_len)
{
    if(sctp_sendmsg(server_fd, payload, payload_len, (struct sockaddr*)dest_addr, sizeof(struct sockaddr_in), 0, 0, 0, 0, 0) == -1) {
        printf("sctp_sendmsg() error\n");
        return 1;
    }

    return 0;
}

int get_reply(int server_fd)
{
    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    int dest_addr_len = sizeof(dest_addr);

    char payload[1024];
    int buffer_len = sizeof(payload) - 1;
    memset(&payload, 0, sizeof(payload));

    struct sctp_sndrcvinfo snd_rcv_info;
    memset(&snd_rcv_info, 0, sizeof(snd_rcv_info));

    int msg_flags = 0;

    while(1) {
        int recv_size = 0;
        if((recv_size = sctp_recvmsg(server_fd, &payload, buffer_len, (struct sockaddr*)&dest_addr, &dest_addr_len, &snd_rcv_info, &msg_flags)) == -1) {
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

void print_usage(const char* binary_name)
{
    printf("SCTP client usage:\n"
           "%s -r <remote IP addresses> -p <port> [-l <local IP addresses>]\n"
           "Where:\n"
           "- remote IP addresses - Semicolon separated list of peer's IP addresses, in single quotes, no tabs/spaces!\n"
           "- port - Server's port to connect to\n"
           "- local IP addresses - Semicolon separated list of local IP addresses to bind to, in single quotes, no tabs spaces\n",
           binary_name);
}
