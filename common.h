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

int handle_notification(union sctp_notification *notif, size_t notif_len)
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

        printf("SCTP_ASSOC_CHANGE notif: state: %s, error code: %d, out streams: %d, in streams: %d, assoc id: %d\n",
               state, n->sac_error, n->sac_outbound_streams, n->sac_inbound_streams, n->sac_assoc_id);

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
        printf("Unhandled notification type %d\n", notif->sn_header.sn_type);
        break;
    }

    return 0;
}
