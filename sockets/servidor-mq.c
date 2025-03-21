#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <errno.h>
#include <pthread.h>
#include <fcntl.h>    // Para O_* 
#include <sys/stat.h> // Para modos
#include "claves.h"

#define SERVER_QUEUE_NAME   "/mq_server"
#define QUEUE_PERMISSIONS   0660
#define MAX_MESSAGES        10

#define MAX_VALUE1_LEN 256
#define MAX_V2 32

typedef enum {
    OP_DESTROY = 0,
    OP_SET,
    OP_GET,
    OP_MODIFY,
    OP_DELETE,
    OP_EXIST
} op_code_t;

// Estructura de mensaje de petición.
typedef struct {
    op_code_t op;
    int key;
    char value1[MAX_VALUE1_LEN];
    int N_value2;
    double V_value2[MAX_V2];
    struct Coord value3;
    char client_queue_name[64];
} request_msg_t;

// Estructura de mensaje de respuesta.
typedef struct {
    int result;
    char value1[MAX_VALUE1_LEN];
    int N_value2;
    double V_value2[MAX_V2];
    struct Coord value3;
} response_msg_t;

void* handle_request(void* arg) {
    request_msg_t req = *((request_msg_t*)arg);
    free(arg);

    printf("[Servidor] Recibida petición: op=%d, key=%d, client_queue=%s\n", req.op, req.key, req.client_queue_name);
    
    response_msg_t resp;
    memset(&resp, 0, sizeof(resp));

    switch(req.op) {
        case OP_DESTROY:
            resp.result = destroy();
            break;
        case OP_SET:
            resp.result = set_value(req.key, req.value1, req.N_value2, req.V_value2, req.value3);
            break;
        case OP_GET:
            resp.result = get_value(req.key, resp.value1, &resp.N_value2, resp.V_value2, &resp.value3);
            break;
        case OP_MODIFY:
            resp.result = modify_value(req.key, req.value1, req.N_value2, req.V_value2, req.value3);
            break;
        case OP_DELETE:
            resp.result = delete_key(req.key);
            break;
        case OP_EXIST:
            resp.result = exist(req.key);
            break;
        default:
            resp.result = -1;
            break;
    }
    
    printf("[Servidor] Enviando respuesta: result=%d\n", resp.result);
    
    mqd_t client_q = mq_open(req.client_queue_name, O_WRONLY);
    if (client_q == (mqd_t)-1) {
        perror("mq_open (cliente)");
        pthread_exit(NULL);
    }
    if (mq_send(client_q, (char *)&resp, sizeof(resp), 0) == -1) {
        perror("mq_send (cliente)");
    }
    mq_close(client_q);
    pthread_exit(NULL);
}

int main(void) {
    mqd_t server_q;
    struct mq_attr attr;

    attr.mq_flags = 0;
    attr.mq_maxmsg = MAX_MESSAGES;
    attr.mq_msgsize = sizeof(request_msg_t);
    attr.mq_curmsgs = 0;

    mq_unlink(SERVER_QUEUE_NAME);
    server_q = mq_open(SERVER_QUEUE_NAME, O_RDONLY | O_CREAT, QUEUE_PERMISSIONS, &attr);
    if (server_q == (mqd_t)-1) {
        perror("mq_open (servidor)");
        exit(EXIT_FAILURE);
    }

    printf("Servidor de colas de mensajes iniciado. Esperando peticiones...\n");

    while (1) {
        request_msg_t *req = malloc(sizeof(request_msg_t));
        if (!req) {
            perror("malloc");
            continue;
        }
        ssize_t bytes_read = mq_receive(server_q, (char *)req, sizeof(request_msg_t), NULL);
        if (bytes_read >= 0) {
            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_request, req) != 0) {
                perror("pthread_create");
                free(req);
            } else {
                pthread_detach(thread);
            }
        } else {
            perror("mq_receive");
            free(req);
        }
    }

    mq_close(server_q);
    mq_unlink(SERVER_QUEUE_NAME);
    return 0;
}
