#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <mqueue.h>
#include <unistd.h>
#include "claves.h"

#define SERVER_QUEUE "/mq_server"         // Asegurarse de usar el mismo nombre que el servidor.
#define CLIENT_QUEUE "/client_queue_%d"     // Nombre de la cola del cliente.
#define MAX_MSG_SIZE 1024

// Estructura del mensaje de petición/respuesta.
// Se ha añadido el campo client_queue_name para que el servidor sepa dónde responder.
typedef struct {
    int operation;             // Código de operación (por ejemplo, 0: destroy, 1: set, etc.)
    int key;
    char value1[256];
    int N_value2;
    double V_value2[32];
    struct Coord value3;
    char client_queue_name[64]; // Nombre de la cola de respuesta del cliente.
    int result;                // Campo para la respuesta (lo utiliza el servidor para enviar el resultado)
} message_t;

int send_request(message_t *request, message_t *response) {
    mqd_t server_q, client_q;
    char client_queue_name[64];
    // Genera un nombre único para la cola del cliente basado en el PID.
    sprintf(client_queue_name, CLIENT_QUEUE, getpid());
    
    // Copiar el nombre de la cola en el mensaje de petición.
    strncpy(request->client_queue_name, client_queue_name, sizeof(request->client_queue_name)-1);
    request->client_queue_name[sizeof(request->client_queue_name)-1] = '\0';

    struct mq_attr attr = {0, 10, MAX_MSG_SIZE, 0};
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
    
    if (mq_send(server_q, (char *)request, sizeof(message_t), 0) == -1) {
        perror("mq_send (servidor)");
        mq_close(client_q);
        mq_unlink(client_queue_name);
        return -2;
    }
    
    if (mq_receive(client_q, (char *)response, sizeof(message_t), NULL) == -1) {
        perror("mq_receive (cliente)");
        mq_close(client_q);
        mq_unlink(client_queue_name);
        return -2;
    }
    
    mq_close(client_q);
    mq_unlink(client_queue_name);
    return response->result;
}
