#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h> // Se agregó para getpid()
#include "claves.h"

#define SERVER_QUEUE "/server_queue"
#define CLIENT_QUEUE "/client_queue_%d"
#define MAX_MSG_SIZE 1024

typedef struct {
    long type;
    int key;
    char value1[256];
    int N_value2;
    double V_value2[32];
    struct Coord value3;
    int operation;
    int result;
} message_t;

int send_request(message_t *request, message_t *response) {
    mqd_t server_queue, client_queue;
    char client_queue_name[64];
    sprintf(client_queue_name, CLIENT_QUEUE, getpid());
    
    struct mq_attr attr = {0, 10, MAX_MSG_SIZE, 0};
    client_queue = mq_open(client_queue_name, O_CREAT | O_RDONLY, 0666, &attr);
    if (client_queue == -1) return -2;
    
    server_queue = mq_open(SERVER_QUEUE, O_WRONLY);
    if (server_queue == -1) {
        mq_close(client_queue);
        mq_unlink(client_queue_name);
        return -2;
    }
    
    if (mq_send(server_queue, (char *)request, sizeof(message_t), 0) == -1) {
        mq_close(client_queue);
        mq_unlink(client_queue_name);
        return -2;
    }
    
    if (mq_receive(client_queue, (char *)response, sizeof(message_t), NULL) == -1) {
        mq_close(client_queue);
        mq_unlink(client_queue_name);
        return -2;
    }
    
    mq_close(client_queue);
    mq_unlink(client_queue_name);
    return response->result;
}
