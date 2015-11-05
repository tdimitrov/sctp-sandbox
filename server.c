#include "common.h"
#include <pthread.h>
#include <errno.h>

int get_connection(int server_fd);
int handle_client(int server_fd, int assoc_id);
void* client_thread(void* client_conn_fd);

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

    //enable some notifications
    if(enable_notifications(server_fd) != 0) {
        printf("Error enabling notifications\n");
        return 4;
    }

    struct sockaddr_in bind_addr;
    memset(&bind_addr, 0, sizeof(struct sockaddr_in));
    bind_addr.sin_family = ADDR_FAMILY;
    bind_addr.sin_port = htons(server_port);
    bind_addr.sin_addr.s_addr = INADDR_ANY;

    if(sctp_bindx(server_fd, (struct sockaddr*)&bind_addr, 1, SCTP_BINDX_ADD_ADDR) == -1) {
        perror("bind");
        return 5;
    }

    if(listen(server_fd, SERVER_LISTEN_QUEUE_SIZE) != 0) {
        perror("listen");
        return 6;
    }

    printf("Listening on port %d\n", server_port);

    while(1) {
        if(get_connection(server_fd)) {
            break;
        }
    }

    return 7;
}

int get_connection(int server_fd)
{
    int sender_addr_size = sizeof(struct sockaddr_in);
    char payload[1024];

    int buffer_len = sizeof(payload) - 1;
    memset(&payload, 0, sizeof(payload));

    struct sockaddr_in sender_addr;
    memset(&sender_addr, 0, sender_addr_size);

    struct sctp_sndrcvinfo snd_rcv_info;
    memset(&snd_rcv_info, 0, sizeof(snd_rcv_info));

    int msg_flags = 0;

    while(1) {
        int recv_size = 0;

        if((recv_size = sctp_recvmsg(server_fd, &payload, buffer_len, (struct sockaddr*)&sender_addr, &sender_addr_size, &snd_rcv_info, &msg_flags)) == -1) {
            printf("recvmsg() error\n");
            return 1;
        }

        if(!(msg_flags & MSG_NOTIFICATION)) {
            printf("Warning! Received payload in client handling loop!!!\n");
            break;
        }

        if(!(msg_flags & MSG_EOR)) {
            printf("Notification received, but the buffer is not big enough.\n");
            continue;
        }

        int assoc_id = get_association_id((union sctp_notification*)payload, recv_size);
        if(assoc_id < 0) {
            printf("Error getting association id\n");
            break;
        }

        if(handle_client(server_fd, assoc_id) < 0) {
            printf("Error handling client\n");
            break;
        }


    }

    return 0;
}


int handle_client(int server_fd, int assoc_id)
{
    int* client_conn_fd = NULL;

    if((client_conn_fd = (int*)calloc(1, sizeof(int))) == NULL) {
        printf("Error allocating memory\n");
        return 1;
    }

    *client_conn_fd = sctp_peeloff(server_fd, assoc_id);
    if(*client_conn_fd == -1) {
        printf("Error in peeloff\n");
        free(client_conn_fd);
        return 2;
    }

    if(disable_notifications(*client_conn_fd) != 0) {
        printf("Error disabling notifications\n");
        free(client_conn_fd);
        return 3;
    }

    pthread_attr_t thread_attr;
    memset(&thread_attr, 0, sizeof thread_attr);

    if(pthread_attr_init(&thread_attr)) {
        printf("Error initialising thread attributes\n");
        free(client_conn_fd);
        return 4;
    }

    if(pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED)) {
        printf("Error setting detached attribute to thread\n");
        free(client_conn_fd);
        return 5;
    }

    pthread_t new_thread;
    memset(&new_thread, 0, sizeof(new_thread));

    if(pthread_create(&new_thread, &thread_attr, &client_thread, (void*)client_conn_fd)) {
        printf("Error creating thread\n");
        free(client_conn_fd);
        return 6;
    }

    return 0;
}

void* client_thread(void* client_conn_fd)
{
    //save the fd to local variable, to avoid memory leaks
    int fd = *(int*)client_conn_fd;
    free(client_conn_fd);

    while (1) {
        const int buf_size = 1024;
        char buf[buf_size];
        memset(buf, 0, buf_size);

        int recv_size = recv(fd, buf, buf_size-1, 0);

        if(recv_size == -1) {
            if(errno == ENOTCONN) {
                printf("Client closed the connection\n");
                return NULL;
            }

            perror("recv()");
            return NULL;
        }
        else if (recv_size == 0) {
            printf("Client closed the connection\n");
            return NULL;
        }

        printf("%d: %s\n",recv_size, buf);

        if(send(fd, "OK", 2, 0) == -1) {
            perror("send()");
            return NULL;
        }
    }
    return NULL;
}
