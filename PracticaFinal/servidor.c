// server.c — Servidor P2P para registro, gestión de usuarios y contenidos
// Compilar con: gcc -pthread -Wall -Wextra -o server server.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <pthread.h>

#define SERVER_PORT       8080
#define BACKLOG           10
#define MSG_BUFFER_SIZE   2048

#define MAX_USERS             100
#define MAX_FILES_PER_USER    100
#define MAX_NAME_LEN          64
#define MAX_PATH_LEN         256
#define MAX_DESC_LEN         256

// Estructuras de datos
typedef struct {
    char path[MAX_PATH_LEN];
    char desc[MAX_DESC_LEN];
} file_t;

typedef struct {
    char name[MAX_NAME_LEN];
    char ip[INET_ADDRSTRLEN];
    int  port;
    int  connected;            // 0 = no, 1 = sí
    int  file_count;
    file_t files[MAX_FILES_PER_USER];
} user_t;

// Tabla global de usuarios
static user_t users[MAX_USERS];
static int    user_count = 0;

// Mutex para proteger acceso a users[]
static pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

// -----------------------------------------------------------------------------
// Handler de cada conexión cliente
// -----------------------------------------------------------------------------
void *handle_client(void *arg) {
    int client_fd = *(int*)arg;
    free(arg);

    char rbuf[MSG_BUFFER_SIZE];
    ssize_t rlen = recv(client_fd, rbuf, sizeof rbuf, 0);
    if (rlen <= 0) {
        close(client_fd);
        return NULL;
    }

    // Parsear tokens separados por '\0'
    char *args[5];
    int   argc = 0;
    char *p = rbuf;
    while (argc < 5 && p < rbuf + rlen) {
        args[argc++] = p;
        p += strlen(p) + 1;
    }

    // Log de operación
    if (argc >= 2) {
        printf("OPERATION %s FROM %s\ns > ", args[0], args[1]);
        fflush(stdout);
    }

    int code = 4;  // código de fallo genérico por defecto

    // ---------------------- REGISTER <user> ----------------------
    if (argc == 2 && strcmp(args[0], "REGISTER") == 0) {
        char *user = args[1];
        pthread_mutex_lock(&server_mutex);
        int found = 0;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, user) == 0) {
                found = 1;
                break;
            }
        }
        if (found) {
            code = 1;  // usuario ya existe
        } else if (user_count < MAX_USERS) {
            strncpy(users[user_count].name, user, MAX_NAME_LEN);
            users[user_count].connected  = 0;
            users[user_count].file_count = 0;
            user_count++;
            code = 0;  // OK
        }
        pthread_mutex_unlock(&server_mutex);

    // --------------------- UNREGISTER <user> ---------------------
    } else if (argc == 2 && strcmp(args[0], "UNREGISTER") == 0) {
        char *user = args[1];
        pthread_mutex_lock(&server_mutex);
        int idx = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, user) == 0) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            code = 1;  // no existe
        } else {
            for (int j = idx; j < user_count-1; j++) {
                users[j] = users[j+1];
            }
            user_count--;
            code = 0;  // OK
        }
        pthread_mutex_unlock(&server_mutex);

    // ---------------- CONNECT <user> <port> ----------------
    } else if (argc == 3 && strcmp(args[0], "CONNECT") == 0) {
        char *user = args[1];
        int   port = atoi(args[2]);
        struct sockaddr_in peer_addr;
        socklen_t addrlen = sizeof peer_addr;
        getpeername(client_fd, (struct sockaddr*)&peer_addr, &addrlen);
        char peer_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer_addr.sin_addr, peer_ip, sizeof peer_ip);

        pthread_mutex_lock(&server_mutex);
        int idx = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, user) == 0) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            code = 1;  // USER DOES NOT EXIST
        } else if (users[idx].connected) {
            code = 2;  // USER ALREADY CONNECTED
        } else {
            users[idx].connected = 1;
            users[idx].port      = port;
            strncpy(users[idx].ip, peer_ip, sizeof users[idx].ip);
            code = 0;  // CONNECT OK
        }
        pthread_mutex_unlock(&server_mutex);

    // --------------- DISCONNECT <user> -----------------
    } else if (argc == 2 && strcmp(args[0], "DISCONNECT") == 0) {
        char *user = args[1];
        pthread_mutex_lock(&server_mutex);
        int idx = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, user) == 0) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            code = 1;  // USER DOES NOT EXIST
        } else if (!users[idx].connected) {
            code = 2;  // USER NOT CONNECTED
        } else {
            users[idx].connected = 0;
            code = 0;  // DISCONNECT OK
        }
        pthread_mutex_unlock(&server_mutex);

    // ----------- PUBLISH <user> <path> <desc> -----------
    } else if (argc == 4 && strcmp(args[0], "PUBLISH") == 0) {
        char *user = args[1];
        char *path = args[2];
        char *desc = args[3];
        pthread_mutex_lock(&server_mutex);
        int idx = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, user) == 0) {
                idx = i; break;
            }
        }
        if (idx < 0) {
            code = 1;  // USER DOES NOT EXIST
        } else if (!users[idx].connected) {
            code = 2;  // USER NOT CONNECTED
        } else {
            int exists = 0;
            for (int f = 0; f < users[idx].file_count; f++) {
                if (strcmp(users[idx].files[f].path, path) == 0) {
                    exists = 1; break;
                }
            }
            if (exists) {
                code = 3;  // CONTENT ALREADY PUBLISHED
            } else if (users[idx].file_count < MAX_FILES_PER_USER) {
                strncpy(users[idx].files[users[idx].file_count].path, path, MAX_PATH_LEN);
                strncpy(users[idx].files[users[idx].file_count].desc, desc, MAX_DESC_LEN);
                users[idx].file_count++;
                code = 0;  // PUBLISH OK
            }
        }
        pthread_mutex_unlock(&server_mutex);

    // ---------- DELETE <user> <path> ----------
    } else if (argc == 3 && strcmp(args[0], "DELETE") == 0) {
        char *user = args[1];
        char *path = args[2];
        pthread_mutex_lock(&server_mutex);
        int idx = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, user) == 0) {
                idx = i; break;
            }
        }
        if (idx < 0) {
            code = 1;  // USER DOES NOT EXIST
        } else if (!users[idx].connected) {
            code = 2;  // USER NOT CONNECTED
        } else {
            int found = -1;
            for (int f = 0; f < users[idx].file_count; f++) {
                if (strcmp(users[idx].files[f].path, path) == 0) {
                    found = f; break;
                }
            }
            if (found < 0) {
                code = 3;  // CONTENT NOT PUBLISHED
            } else {
                for (int f = found; f < users[idx].file_count-1; f++) {
                    users[idx].files[f] = users[idx].files[f+1];
                }
                users[idx].file_count--;
                code = 0;  // DELETE OK
            }
        }
        pthread_mutex_unlock(&server_mutex);

    // -------- LIST_USERS <user> --------
    } else if (argc == 2 && strcmp(args[0], "LIST_USERS") == 0) {
        char *user = args[1];
        pthread_mutex_lock(&server_mutex);
        int idx = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, user) == 0) {
                idx = i; break;
            }
        }
        if (idx < 0) {
            code = 1;  // USER DOES NOT EXIST
        } else if (!users[idx].connected) {
            code = 2;  // USER NOT CONNECTED
        } else {
            code = 0;  // OK
        }
        int n_connected = 0;
        char listbuf[MSG_BUFFER_SIZE];
        if (code == 0) {
            for (int i = 0; i < user_count; i++) {
                if (users[i].connected) n_connected++;
            }
            snprintf(listbuf, sizeof listbuf, "%d", n_connected);
        }
        pthread_mutex_unlock(&server_mutex);

        send(client_fd, &(char){(char)code}, 1, 0);
        if (code == 0) {
            send(client_fd, listbuf, strlen(listbuf)+1, 0);
            pthread_mutex_lock(&server_mutex);
            for (int i = 0; i < user_count; i++) {
                if (!users[i].connected) continue;
                send(client_fd, users[i].name, strlen(users[i].name)+1, 0);
                send(client_fd, users[i].ip,   strlen(users[i].ip)+1,   0);
                char portbuf[16];
                snprintf(portbuf, sizeof portbuf, "%d", users[i].port);
                send(client_fd, portbuf, strlen(portbuf)+1, 0);
            }
            pthread_mutex_unlock(&server_mutex);
            close(client_fd);
            return NULL;
        }

    // ---- LIST_CONTENT <user> <target> ----
    } else if (argc == 3 && strcmp(args[0], "LIST_CONTENT") == 0) {
        char *user   = args[1];
        char *target = args[2];
        pthread_mutex_lock(&server_mutex);
        int idx_u = -1, idx_t = -1;
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, user  ) == 0) idx_u = i;
            if (strcmp(users[i].name, target) == 0) idx_t = i;
        }
        if (idx_u < 0) {
            code = 1;  // USER DOES NOT EXIST
        } else if (!users[idx_u].connected) {
            code = 2;  // USER NOT CONNECTED
        } else if (idx_t < 0) {
            code = 3;  // REMOTE USER DOES NOT EXIST
        } else {
            code = 0;  // OK
        }
        int n_files = (code == 0 ? users[idx_t].file_count : 0);
        char listbuf[MSG_BUFFER_SIZE];
        if (code == 0) {
            snprintf(listbuf, sizeof listbuf, "%d", n_files);
        }
        pthread_mutex_unlock(&server_mutex);

        send(client_fd, &(char){(char)code}, 1, 0);
        if (code == 0) {
            send(client_fd, listbuf, strlen(listbuf)+1, 0);
            pthread_mutex_lock(&server_mutex);
            for (int f = 0; f < users[idx_t].file_count; f++) {
                send(client_fd,
                     users[idx_t].files[f].path,
                     strlen(users[idx_t].files[f].path)+1, 0);
            }
            pthread_mutex_unlock(&server_mutex);
        }

    } else {
        code = 4;  // operación no reconocida
    }

    // Enviar solo el byte de código para el resto de ops
    send(client_fd, &(char){(char)code}, 1, 0);
    close(client_fd);
    return NULL;
}

// -----------------------------------------------------------------------------
// main — inicializa servidor, bucle de accept y threads
// -----------------------------------------------------------------------------
int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        switch (opt) {
            case 'p': port = atoi(optarg); break;
            default:
                fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(port);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof addr) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }
    if (listen(server_fd, BACKLOG) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    // Obtener IP local para el mensaje de init
    struct sockaddr_in sin;
    socklen_t len = sizeof sin;
    if (getsockname(server_fd, (struct sockaddr*)&sin, &len) == -1) {
        perror("getsockname");
        exit(EXIT_FAILURE);
    }
    char local_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &sin.sin_addr, local_ip, sizeof local_ip);

    printf("s > init server %s:%d\ns > ", local_ip, port);
    fflush(stdout);

    // Bucle principal
    while (1) {

        int *client_fd = malloc(sizeof *client_fd);
        *client_fd = accept(server_fd, NULL, NULL);
        if (*client_fd < 0) {
            perror("accept");
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
