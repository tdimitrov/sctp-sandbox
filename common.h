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
        char peer_addresses[256];
        struct sctp_assoc_change* n = &notif->sn_assoc_change;

        switch(n->sac_state) {
        case SCTP_COMM_UP:
            state = "COMM UP";
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

        get_assoc_local_addresses(n->sac_assoc_id, socket, local_addresses, sizeof(local_addresses));
        get_assoc_peer_addresses(n->sac_assoc_id, socket, peer_addresses, sizeof(peer_addresses));

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
