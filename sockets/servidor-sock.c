#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "claves.h"

#define SERVER_PORT 8080           // Valor por defecto; se puede cambiar por parámetro
#define BACKLOG 10
#define MSG_BUFFER_SIZE 2048
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
    char client_queue_name[64];  // No se usa en sockets; se puede dejar vacío o ignorar
} request_msg_t;

// Estructura de mensaje de respuesta.
typedef struct {
    int result;
    char value1[MAX_VALUE1_LEN];
    int N_value2;
    double V_value2[MAX_V2];
    struct Coord value3;
} response_msg_t;

///////////////// FUNCIONES DE CONVERSIÓN //////////////////
// Estas funciones convierten entre struct y cadena (separados por '|')
// Se asume que tanto string_to_request como response_to_string están correctamente implementadas.
void request_to_string(request_msg_t *req, char *buffer, size_t size) {
    char doubles[2048] = "";
    for (int i = 0; i < req->N_value2; i++) {
        uint32_t high = (uint32_t)(req->V_value2[i]) >> 16;
        uint32_t low = (uint32_t)(req->V_value2[i]) & 0xFFFF;
        char temp[64];
        sprintf(temp, "%u,%u%s", high, low, (i < req->N_value2 - 1) ? "," : "");
        strcat(doubles, temp);
    }
    sprintf(buffer, "%d|%d|%s|%d|%s|%d,%d|%s", 
        req->op, req->key, req->value1, req->N_value2, doubles, req->value3.x, req->value3.y, ""); 
    // El último campo no es usado en sockets
}

void string_to_request(char *buffer, request_msg_t *req) {
    char *token = strtok(buffer, "|");
    if (!token) return;
    req->op = atoi(token);

    token = strtok(NULL, "|");
    if (!token) return;
    req->key = atoi(token);

    token = strtok(NULL, "|");
    if (!token) return;
    strncpy(req->value1, token, MAX_VALUE1_LEN-1);
    req->value1[MAX_VALUE1_LEN-1] = '\0';

    token = strtok(NULL, "|");
    if (!token) return;
    req->N_value2 = atoi(token);

    token = strtok(NULL, "|");
    if (!token) return;
    {
        int count = 0;
        char *num = strtok(token, ",");
        while (num && count < req->N_value2 && count < MAX_V2) {
            uint32_t high = atoi(num);
            num = strtok(NULL, ",");
            if (!num) break;
            uint32_t low = atoi(num);
            req->V_value2[count++] = ((uint8_t)high << 16) | low;
            num = strtok(NULL, ",");
        }
    }

    token = strtok(NULL, "|");
    if (!token) return;
    {
        char *xStr = strtok(token, ",");
        char *yStr = strtok(NULL, ",");
        if (xStr && yStr) {
            req->value3.x = atoi(xStr);
            req->value3.y = atoi(yStr);
        }
    }
}

void response_to_string(response_msg_t *resp, char *buffer, size_t size) {
    char doubles[2048] = "";
    for (int i = 0; i < resp->N_value2; i++) {
        uint32_t high = (uint32_t)(resp->V_value2[i]) >> 16;
        uint32_t low = (uint32_t)(resp->V_value2[i]) & 0xFFFF;
        char temp[64];
        sprintf(temp, "%u,%u%s", high, low, (i < resp->N_value2 - 1)? "," : "");
        strcat(doubles, temp);
    }
    sprintf(buffer, "%d|%s|%d|%s|%d,%d", 
        resp->result, resp->value1, resp->N_value2, doubles, resp->value3.x, resp->value3.y);
}

///////////////// FIN FUNCIONES DE CONVERSIÓN //////////////////

static pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

void* handle_request(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);
    char buffer[MSG_BUFFER_SIZE] = "";
    ssize_t total = 0;
    
    // Bloqueo global para evitar lecturas/parseos concurrentes
    pthread_mutex_lock(&server_mutex);
    while(1) {
        ssize_t n = recv(client_fd, buffer + total, sizeof(buffer) - 1 - total, 0);
        if(n <= 0) {
            perror("recv");
            close(client_fd);
            pthread_mutex_unlock(&server_mutex);
            pthread_exit(NULL);
        }
        total += n;
        buffer[total] = '\0';
        // Aquí podrías detectar un fin lógico (por ejemplo, si includes un delimitador)
        // o simplemente romper si no esperas más datos.
        // Para ejemplo:
        break; 
    }
    
    request_msg_t req;
    string_to_request(buffer, &req);
    printf("[Servidor] Recibida petición: op=%d, key=%d\n", req.op, req.key);
    
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
    memset(buffer, 0, sizeof(buffer));
    response_to_string(&resp, buffer, sizeof(buffer));
    if(send(client_fd, buffer, strlen(buffer), 0) != (ssize_t)strlen(buffer)) {
        perror("send");
    }
    close(client_fd);
    pthread_mutex_unlock(&server_mutex);
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    int server_fd;
    struct sockaddr_in server_addr;
    int port = SERVER_PORT;
    if(argc >= 2) {
        port = atoi(argv[1]);
    }
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1) {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    // Escuchar en todas las interfaces
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("bind");
        printf("Error: No se pudo enlazar al puerto %d. Intentando con un puerto dinámico...\n", port);
        server_addr.sin_port = 0; // Permitir que el sistema asigne un puerto dinámico
        if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
            perror("bind (puerto dinámico)");
            exit(EXIT_FAILURE);
        }
        // Obtener el puerto asignado dinámicamente
        socklen_t addr_len = sizeof(server_addr);
        if(getsockname(server_fd, (struct sockaddr *)&server_addr, &addr_len) == -1) {
            perror("getsockname");
            exit(EXIT_FAILURE);
    }
    port = ntohs(server_addr.sin_port);
        printf("Puerto asignado dinámicamente: %d\n", port);
    }
    if(listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    
    printf("Servidor de sockets iniciado. Escuchando en el puerto %d...\n", port);
    
    while(1) {
        int *client_fd = malloc(sizeof(int));
        if(!client_fd) {
            perror("malloc");
            continue;
        }
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if(*client_fd == -1) {
            perror("accept");
            free(client_fd);
            continue;
        }
        pthread_t thread;
        if(pthread_create(&thread, NULL, handle_request, client_fd) != 0) {
            perror("pthread_create");
            close(*client_fd);
            free(client_fd);
        } else {
            pthread_detach(thread);
        }
    }
    
    close(server_fd);
    return 0;
}
