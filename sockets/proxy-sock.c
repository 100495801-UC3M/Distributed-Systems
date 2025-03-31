#ifndef MAX_VALUE1_LEN
#define MAX_VALUE1_LEN 256
#endif
#ifndef MAX_V2
#define MAX_V2 32
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "claves.h"
#include <math.h>
#include <pthread.h>
#include <netdb.h>

#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 8080

#define MSG_BUFFER_SIZE 2048

// Variables globales para la configuración del servidor
static char g_server_ip[MAX_VALUE1_LEN] = DEFAULT_SERVER_IP;
static int g_server_port = DEFAULT_SERVER_PORT;

// Mutex global para sincronización
static pthread_mutex_t proxy_mutex = PTHREAD_MUTEX_INITIALIZER;

// Función de inicialización a llamar desde la aplicación cliente
void init_proxy(const char *ip, int port) {
    const char *env_port = getenv("SERVER_PORT");
    if (env_port) {
        g_server_port = atoi(env_port);
    } else if (port > 0) {
        g_server_port = port;
    }
    if (ip) {
        strncpy(g_server_ip, ip, MAX_VALUE1_LEN-1);
        g_server_ip[MAX_VALUE1_LEN-1] = '\0';
    }
}

// Definiciones compartidas
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

///////////////// FUNCIONES DE CONVERSIÓN //////////////////
// Se copia el código utilizado en servidor-sock.c

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
        req->op, req->key, req->value1, req->N_value2, doubles, req->value3.x, req->value3.y, req->client_queue_name);
}

void response_to_string(response_msg_t *resp, char *buffer, size_t size) {
    char doubles[2048] = "";
    for (int i = 0; i < resp->N_value2; i++) {
        uint32_t high = (uint32_t)(resp->V_value2[i]) >> 16;
        uint32_t low = (uint32_t)(resp->V_value2[i]) & 0xFFFF;
        char temp[64];
        sprintf(temp, "%u,%u%s", high, low, (i < resp->N_value2 - 1) ? "," : "");
        strcat(doubles, temp);
    }
    sprintf(buffer, "%d|%s|%d|%s|%d,%d", 
        resp->result, resp->value1, resp->N_value2, doubles, resp->value3.x, resp->value3.y);
}

void string_to_response(char *buffer, response_msg_t *resp) {
    char *token = strtok(buffer, "|");
    if (!token) return;
    resp->result = atoi(token);

    token = strtok(NULL, "|");
    if (!token) return;
    strncpy(resp->value1, token, MAX_VALUE1_LEN-1);
    resp->value1[MAX_VALUE1_LEN-1] = '\0';

    token = strtok(NULL, "|");
    if (!token) return;
    int expected = atoi(token);
    resp->N_value2 = expected;

    token = strtok(NULL, "|");
    if (!token) return;
    {
        int count = 0;
        char *num = strtok(token, ",");
        while (num && count < expected && count < MAX_V2) {
            uint32_t high = atoi(num);
            num = strtok(NULL, ",");
            if (!num) break;
            uint32_t low = atoi(num);
            resp->V_value2[count++] = ((uint8_t)high << 16) | low;
            num = strtok(NULL, ",");
        }
    }

    token = strtok(NULL, "|");
    if (token) {
        char *xStr = strtok(token, ",");
        char *yStr = strtok(NULL, ",");
        if (xStr && yStr) {
            resp->value3.x = atoi(xStr);
            resp->value3.y = atoi(yStr);
        }
    }
}
///////////////////////////////////////////////////////////

// Función interna para enviar una petición y esperar la respuesta.
static int send_request(request_msg_t *request, response_msg_t *response) {
    pthread_mutex_lock(&proxy_mutex);
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        perror("socket");
        pthread_mutex_unlock(&proxy_mutex);
        return -2;
    }
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;

    // Resolver el nombre del host usando gethostbyname
    struct hostent *host = gethostbyname(g_server_ip);
    if (!host) {
        perror("gethostbyname");
        printf("Error: No se pudo resolver el host '%s'\n", g_server_ip);
        close(sockfd);
        pthread_mutex_unlock(&proxy_mutex);
        return -2;
    }
    memcpy(&serv_addr.sin_addr, host->h_addr_list[0], host->h_length);
    serv_addr.sin_port = htons(g_server_port);

    printf("Proxy: conectando a %s:%d\n", g_server_ip, g_server_port);
    if(connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == -1) {
        perror("connect");
        printf("Error: No se pudo conectar al servidor en %s:%d\n", g_server_ip, g_server_port);
        close(sockfd);
        pthread_mutex_unlock(&proxy_mutex);
        return -2;
    }
    
    char buffer[MSG_BUFFER_SIZE] = "";
    // Convertir la request a cadena y enviar
    request_to_string(request, buffer, sizeof(buffer));
    if(send(sockfd, buffer, strlen(buffer), 0) != (ssize_t)strlen(buffer)) {
        perror("send");
        close(sockfd);
        pthread_mutex_unlock(&proxy_mutex);
        return -2;
    }
    
    // Recibir la respuesta como cadena
    memset(buffer, 0, sizeof(buffer));
    ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
    if(n <= 0) {
        perror("recv");
        close(sockfd);
        pthread_mutex_unlock(&proxy_mutex);
        return -2;
    }
    buffer[n] = '\0';
    string_to_response(buffer, response);
    
    close(sockfd);
    pthread_mutex_unlock(&proxy_mutex);
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
    if (N_value2 < 1 || N_value2 > MAX_V2) {
        return -1;
    }
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
    if (N_value2 < 1 || N_value2 > MAX_V2) {
        return -1;
    }
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
