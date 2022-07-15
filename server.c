#include "common.h"
#include <pthread.h>

typedef struct _client_data {
    int socket_fd;
    struct sockaddr addr;
} client_data_t;

void *handle_connection(void *thread_data);

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
        client_data_t* client_data = NULL;

        if((client_data = malloc(sizeof(client_data_t))) == NULL)
        {
            printf("Error in memory allocation\n");
            return 6;
        }

        memset(client_data, 0, sizeof(client_data_t));
        unsigned int client_addr_len = sizeof(client_data->addr);

        if((client_data->socket_fd = accept(server_fd, &(client_data->addr), &client_addr_len)) == -1) {
            perror("accept");
            free(client_data);
            return 7;
        }

        pthread_t conn_thread;
        pthread_attr_t thread_attr;

        memset(&conn_thread, 0, sizeof(conn_thread));
        memset(&thread_attr, 0, sizeof(thread_attr));

        if(pthread_attr_init(&thread_attr)) {
            printf("Error occureed while initializing thread attributes structure\n");
            free(client_data);
            return 8;
        }
        if(pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_DETACHED)) {
            printf("Error occureed while setting thread attributes\n");
            free(client_data);
            return 9;
        }

        if(pthread_create(&conn_thread, &thread_attr, &handle_connection, (void*)client_data)) {
            printf("Error creating thread\n");
            free(client_data);
            return 10;
        }
    }

    return 0;
}

void* handle_connection(void* thread_data)
{
    client_data_t* client_data = (client_data_t*)thread_data;
    struct sockaddr_in* p_addr = (struct sockaddr_in*) &client_data->addr;	//cast the addr to sockaddr_in
    int socket = client_data->socket_fd;
    char client_ipaddr[128];
    memset(client_ipaddr, 0, sizeof(client_ipaddr));

    if(inet_ntop(AF_INET, &p_addr->sin_addr, client_ipaddr, sizeof(client_ipaddr)) == NULL) {
        perror("inet_ntop");
        return NULL;
    }

    free(client_data);

    printf("Got connection from %s\n", client_ipaddr);

    while(1) {
        char buf[1024];
        int recv_len = 0;

        if((recv_len = recv(socket, &buf, sizeof(buf), 0)) == -1) {
            perror("recv");
            return NULL;
        }

        if(recv_len == 0) {
            printf("Connection from %s closed by remote peer.\n", client_ipaddr);
            if(close(socket) == -1) {
                perror("close");
            }
            return NULL;
        }

        printf("Message received: %s\n", buf);

        strncpy(buf, "OK", sizeof(buf)-1);

        if(send(socket, &buf, strlen(buf), 0) == -1) {
            perror("send");
            return NULL;
        }
    }
}
