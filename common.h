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
const int MAX_MULTIHOMING_ADDRESSES = 4;

int enable_notifications(int fd)
{
    struct sctp_event_subscribe events_subscr;
    memset(&events_subscr, 0, sizeof(events_subscr));

    events_subscr.sctp_association_event = 1;
    events_subscr.sctp_shutdown_event = 1;

    return setsockopt(fd, IPPROTO_SCTP, SCTP_EVENTS, &events_subscr, sizeof(events_subscr));
}

void fill_addresses(struct sockaddr *addrs, int addr_count, char* text_buf, int text_buf_size)
{
    for(int i = 0; i < addr_count; i++) {
        char ip_addr[16];
        memset(ip_addr, 0, sizeof(ip_addr));

        struct sockaddr_in* addr_in = ( struct sockaddr_in*)&addrs[i];  //address part should be passed to inet_ntop
        inet_ntop(ADDR_FAMILY, &addr_in->sin_addr, ip_addr, sizeof(ip_addr));

        int curr_text_buf_len = strlen(text_buf);
        snprintf(text_buf + curr_text_buf_len, text_buf_size - curr_text_buf_len, "%s ", ip_addr);
    }
}

int get_assoc_local_addresses(const sctp_assoc_t assoc_id, const int socket, char* text_buf, int text_buf_size)
{
    struct sockaddr *addrs = NULL;
    int addr_count = -1;

    memset(text_buf, 0, text_buf_size);
    text_buf_size--;    //leave space for the null terminator

    if((addr_count = sctp_getpaddrs(socket, assoc_id, &addrs)) == -1) {
        printf("Error occured while calling sctp_getpaddrs()\n");
        return 1;
    }

    snprintf(text_buf, text_buf_size, "Peer IP addresses: ");

    fill_addresses(addrs, addr_count, text_buf, text_buf_size);

    sctp_freepaddrs(addrs);

    return 0;
}

int get_assoc_peer_addresses(const sctp_assoc_t assoc_id, const int socket, char* text_buf, int text_buf_size)
{
    struct sockaddr *addrs = NULL;
    int addr_count = -1;

    memset(text_buf, 0, text_buf_size);
    text_buf_size--;    //leave space for the null terminator

    if((addr_count = sctp_getladdrs(socket, assoc_id, &addrs)) == -1) {
        printf("Error occured while calling sctp_getpaddrs()\n");
        return 1;
    }

    snprintf(text_buf, text_buf_size, "Local IP addresses: ");

    fill_addresses(addrs, addr_count, text_buf, text_buf_size);

    sctp_freeladdrs(addrs);

    return 0;
}

int handle_notification(union sctp_notification *notif, size_t notif_len, const int socket)
{
    // http://stackoverflow.com/questions/20679070/how-does-one-determine-the-size-of-an-unnamed-struct
    int notif_header_size = sizeof( ((union sctp_notification*)NULL)->sn_header );

    if(notif_header_size > notif_len) {
        printf("Error: Notification msg size is smaller than notification header size!\n");
        return 1;
    }

    switch(notif->sn_header.sn_type) {
    case SCTP_ASSOC_CHANGE: {
        if(sizeof(struct sctp_assoc_change) > notif_len) {
            printf("Error notification msg size is smaller than struct sctp_assoc_change size\n");
            return 2;
        }

        char* state = NULL;

        char local_addresses[256];
        memset(&local_addresses, 0, sizeof(local_addresses));

        char peer_addresses[256];
        memset(&peer_addresses, 0, sizeof(peer_addresses));

        struct sctp_assoc_change* n = &notif->sn_assoc_change;

        switch(n->sac_state) {
        case SCTP_COMM_UP:
            state = "COMM UP";
            get_assoc_local_addresses(n->sac_assoc_id, socket, local_addresses, sizeof(local_addresses));
            get_assoc_peer_addresses(n->sac_assoc_id, socket, peer_addresses, sizeof(peer_addresses));

            break;

        case SCTP_COMM_LOST:
            state = "COMM_LOST";
            break;

        case SCTP_RESTART:
            state = "RESTART";
            break;

        case SCTP_SHUTDOWN_COMP:
            state = "SHUTDOWN_COMP";
            break;

        case SCTP_CANT_STR_ASSOC:
            state = "CAN'T START ASSOC";
            break;
        }

        printf("SCTP_ASSOC_CHANGE notif: state: %s, error code: %d, out streams: %d, in streams: %d, assoc id: %d %s %s\n",
               state, n->sac_error, n->sac_outbound_streams, n->sac_inbound_streams, n->sac_assoc_id, local_addresses, peer_addresses);

        break;
    }

    case SCTP_SHUTDOWN_EVENT: {
        if(sizeof(struct sctp_shutdown_event) > notif_len) {
            printf("Error notification msg size is smaller than struct sctp_assoc_change size\n");
            return 3;
        }

        struct sctp_shutdown_event* n = &notif->sn_shutdown_event;

        printf("SCTP_SHUTDOWN_EVENT notif: assoc id: %d\n", n->sse_assoc_id);
        break;
    }

    default:
        printf("Und notification type %d\n", notif->sn_header.sn_type);
        break;
    }

    return 0;
}

int validate_port_number(int port_number)
{
    if(port_number < 1 || port_number > 65535) {
        printf("Invalid port number (%d). Valid values are between 1 and 65535.\n", port_number);
        return 1;
    }

    return 0;
}

int parse_addresses(char* input, const int input_len, const int port_number, struct sockaddr_in* addresses, const int max_addr_count, int* count)
{
    if(input[input_len] != '\0') {
        printf("parse_addresses(): input is not NULL terminated!\n");
        return 1;
    }

    char* input_addr[max_addr_count];
    int input_addr_count = 0;


    input_addr[0] = strtok(input, ";");

    for(input_addr_count = 1; input_addr_count < max_addr_count; input_addr_count++) {
        input_addr[input_addr_count] = strtok(NULL, ";");

        if(input_addr[input_addr_count] == NULL)
            break;
    }

    for(int i = 0; i < input_addr_count; i++) {
        struct sockaddr_in* bind_addr = &addresses[i];
        memset(bind_addr, 0, sizeof(struct sockaddr_in));
        bind_addr->sin_family = ADDR_FAMILY;
        bind_addr->sin_port = htons(port_number);
        if(inet_pton(AF_INET, input_addr[i], &(bind_addr->sin_addr)) != 1) {
            printf("parse_addresses(): Error converting IP address (%s) to sockaddr_in structure\n", input_addr[i]);
            return 1;
        }
    }

    *count = input_addr_count;

    return 0;
}
