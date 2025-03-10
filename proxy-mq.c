#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h>
#include "claves.h"

#define SERVER_QUEUE "/mq_server"
#define CLIENT_QUEUE "/client_queue_%d"

// Definiciones compartidas
typedef enum {
    OP_DESTROY = 0,
    OP_SET,
    OP_GET,
    OP_MODIFY,
    OP_DELETE,
    OP_EXIST
} op_code_t;

#define MAX_VALUE1_LEN 256
#define MAX_V2 32

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

// Función interna para enviar una petición y esperar la respuesta.
static int send_request(request_msg_t *request, response_msg_t *response) {
    mqd_t server_q, client_q;
    char client_queue_name[64];
    
    // Generar un nombre único para la cola del cliente basado en el PID.
    sprintf(client_queue_name, CLIENT_QUEUE, getpid());
    strncpy(request->client_queue_name, client_queue_name, sizeof(request->client_queue_name)-1);
    request->client_queue_name[sizeof(request->client_queue_name)-1] = '\0';

    // Crear la cola del cliente con tamaño máximo igual al de la respuesta.
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(response_msg_t);
    attr.mq_curmsgs = 0;
    
    client_q = mq_open(client_queue_name, O_CREAT | O_RDONLY, 0666, &attr);
    if (client_q == (mqd_t)-1) {
        perror("mq_open (cliente)");
        return -2;
    }
    
    server_q = mq_open(SERVER_QUEUE, O_WRONLY);
    if (server_q == (mqd_t)-1) {
        perror("mq_open (servidor)");
        mq_close(client_q);
        mq_unlink(client_queue_name);
        return -2;
    }
    
    if (mq_send(server_q, (char *)request, sizeof(request_msg_t), 0) == -1) {
        perror("mq_send (servidor)");
        mq_close(client_q);
        mq_unlink(client_queue_name);
        return -2;
    }
    
    if (mq_receive(client_q, (char *)response, sizeof(response_msg_t), NULL) == -1) {
        perror("mq_receive (cliente)");
        mq_close(client_q);
        mq_unlink(client_queue_name);
        return -2;
    }
    
    mq_close(client_q);
    mq_unlink(client_queue_name);
    return response->result;
}

// Implementación de la API usando el proxy.

int destroy(void) {
    request_msg_t req;
    response_msg_t resp;
    memset(&req, 0, sizeof(req));
    req.op = OP_DESTROY;
    return send_request(&req, &resp);
}

int set_value(int key, char *value1, int N_value2, double *V_value2, struct Coord value3) {
    // Si la cadena es mayor a 255 caracteres, se rechaza.
    if (strlen(value1) > 255)
        return -1;
    request_msg_t req;
    response_msg_t resp;
    memset(&req, 0, sizeof(req));
    req.op = OP_SET;
    req.key = key;
    strncpy(req.value1, value1, sizeof(req.value1)-1);
    req.value1[sizeof(req.value1)-1] = '\0';
    req.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++)
        req.V_value2[i] = V_value2[i];
    req.value3 = value3;
    return send_request(&req, &resp);
}

int get_value(int key, char *value1, int *N_value2, double *V_value2, struct Coord *value3) {
    request_msg_t req;
    response_msg_t resp;
    memset(&req, 0, sizeof(req));
    req.op = OP_GET;
    req.key = key;
    int ret = send_request(&req, &resp);
    if (ret == 0) {
        strncpy(value1, resp.value1, MAX_VALUE1_LEN);
        *N_value2 = resp.N_value2;
        for (int i = 0; i < resp.N_value2; i++)
            V_value2[i] = resp.V_value2[i];
        *value3 = resp.value3;
    }
    return ret;
}

int modify_value(int key, char *value1, int N_value2, double *V_value2, struct Coord value3) {
    // Verificar longitud de la cadena.
    if (strlen(value1) > 255)
        return -1;
    request_msg_t req;
    response_msg_t resp;
    memset(&req, 0, sizeof(req));
    req.op = OP_MODIFY;
    req.key = key;
    strncpy(req.value1, value1, sizeof(req.value1)-1);
    req.value1[sizeof(req.value1)-1] = '\0';
    req.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++)
        req.V_value2[i] = V_value2[i];
    req.value3 = value3;
    return send_request(&req, &resp);
}

int delete_key(int key) {
    request_msg_t req;
    response_msg_t resp;
    memset(&req, 0, sizeof(req));
    req.op = OP_DELETE;
    req.key = key;
    return send_request(&req, &resp);
}

int exist(int key) {
    request_msg_t req;
    response_msg_t resp;
    memset(&req, 0, sizeof(req));
    req.op = OP_EXIST;
    req.key = key;
    return send_request(&req, &resp);
}
