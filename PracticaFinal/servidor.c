// server.c — Servidor P2P para Parte 1 (sesión única)
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
    int  connected;      // 0 = no, 1 = sí
    int  file_count;
    file_t files[MAX_FILES_PER_USER];
} user_t;

// Tabla global de usuarios
static user_t users[MAX_USERS];
static int    user_count = 0;
static pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * parse:
 *   Descompone buf (tokens separados por '\0') en args[],
 *   devolviendo argc.
 */
static int parse(char *buf, int buflen, char **args) {
    int argc = 0;
    char *p = buf;
    while (p < buf + buflen && argc < 10) {
        args[argc++] = p;
        p += strlen(p) + 1;
    }
    return argc;
}

void *handle_client(void *arg) {
    int fd = *(int*)arg;
    free(arg);

    char buf[MSG_BUFFER_SIZE];
    char *args[10];
    char current[MAX_NAME_LEN] = "";  // usuario en sesión
    int session = 1, code;

    while (session) {
        ssize_t r = recv(fd, buf, sizeof buf, 0);
        if (r <= 0) break;
        int argc = parse(buf, r, args);
        code = 4;  // error genérico

        // Log de la operación
        printf("OPERATION %s", args[0]);
        if (current[0]) printf(" FROM %s", current);
        printf("\ns > ");
        fflush(stdout);

        // REGISTER <user>
        if (argc == 2 && strcmp(args[0], "REGISTER") == 0) {
            pthread_mutex_lock(&server_mutex);
            int found = 0;
            for (int i = 0; i < user_count; i++)
                if (!strcmp(users[i].name, args[1])) { found = 1; break; }
            if (found) {
                code = 1;  // usuario ya existe
            } else if (user_count < MAX_USERS) {
                strncpy(users[user_count].name, args[1], MAX_NAME_LEN);
                users[user_count].connected = 0;
                users[user_count].file_count = 0;
                user_count++;
                code = 0;  // OK
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // UNREGISTER <user>
        else if (argc == 2 && strcmp(args[0], "UNREGISTER") == 0) {
            pthread_mutex_lock(&server_mutex);
            int idx = -1;
            for (int i = 0; i < user_count; i++)
                if (!strcmp(users[i].name, args[1])) { idx = i; break; }
            if (idx < 0) {
                code = 1;  // usuario no existe
            } else {
                for (int j = idx; j < user_count-1; j++)
                    users[j] = users[j+1];
                user_count--;
                code = 0;  // OK
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // CONNECT <user> <port>
        else if (argc == 3 && strcmp(args[0], "CONNECT") == 0) {
            char *user = args[1];
            int   port = atoi(args[2]);
            struct sockaddr_in sa; socklen_t len = sizeof sa;
            getpeername(fd, (struct sockaddr*)&sa, &len);
            char peer_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &sa.sin_addr,
                      peer_ip, sizeof peer_ip);

            pthread_mutex_lock(&server_mutex);
            int idx = -1;
            for (int i = 0; i < user_count; i++)
                if (!strcmp(users[i].name, user)) { idx = i; break; }
            if (idx < 0) {
                code = 1;  // USER DOES NOT EXIST
            } else if (users[idx].connected) {
                code = 2;  // USER ALREADY CONNECTED
            } else {
                users[idx].connected = 1;
                strncpy(users[idx].ip, peer_ip, sizeof users[idx].ip);
                users[idx].port = port;
                strncpy(current, user, MAX_NAME_LEN);
                code = 0;  // CONNECT OK
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // PUBLISH <file_name> <description>
        else if (argc == 3 && strcmp(args[0], "PUBLISH") == 0) {
            pthread_mutex_lock(&server_mutex);
            int idx = -1;
            for (int i = 0; i < user_count; i++)
                if (!strcmp(users[i].name, current)) { idx = i; break; }
            if (idx < 0) code = 1;
            else if (!users[idx].connected) code = 2;
            else {
                int exists = 0;
                for (int j = 0; j < users[idx].file_count; j++)
                    if (!strcmp(users[idx].files[j].path, args[1])) {
                        exists = 1; break;
                    }
                if (exists) code = 3;
                else if (users[idx].file_count < MAX_FILES_PER_USER) {
                    strncpy(users[idx].files[users[idx].file_count].path,
                            args[1], MAX_PATH_LEN);
                    strncpy(users[idx].files[users[idx].file_count].desc,
                            args[2], MAX_DESC_LEN);
                    users[idx].file_count++;
                    code = 0;
                }
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // LIST_USERS
        else if (argc == 1 && strcmp(args[0], "LIST_USERS") == 0) {
            pthread_mutex_lock(&server_mutex);
            int idx = -1;
            for (int i = 0; i < user_count; i++)
                if (!strcmp(users[i].name, current)) { idx = i; break; }
            if (idx < 0) {
                code = 1;
            } else if (!users[idx].connected) {
                code = 2;
            } else {
                code = 0;
                int n = 0;
                for (int i = 0; i < user_count; i++)
                    if (users[i].connected) n++;
                send(fd, (char[]){0}, 1, 0);
                char tmp[32];
                snprintf(tmp, sizeof tmp, "%d", n);
                send(fd, tmp, strlen(tmp)+1, 0);
                for (int i = 0; i < user_count; i++) {
                    if (!users[i].connected) continue;
                    send(fd, users[i].name,
                         strlen(users[i].name)+1, 0);
                    send(fd, users[i].ip,
                         strlen(users[i].ip)+1, 0);
                    char pb[16];
                    snprintf(pb, sizeof pb, "%d", users[i].port);
                    send(fd, pb, strlen(pb)+1, 0);
                }
                pthread_mutex_unlock(&server_mutex);
                continue;
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // DELETE <file_name>
        else if (argc == 2 && strcmp(args[0], "DELETE") == 0) {
            pthread_mutex_lock(&server_mutex);
            int idx = -1;
            for (int i = 0; i < user_count; i++)
                if (!strcmp(users[i].name, current)) { idx = i; break; }
            if (idx < 0) {
                code = 1;
            } else if (!users[idx].connected) {
                code = 2;
            } else {
                int fnd = -1;
                for (int j = 0; j < users[idx].file_count; j++)
                    if (!strcmp(users[idx].files[j].path, args[1])) {
                        fnd = j; break;
                    }
                if (fnd < 0) code = 3;
                else {
                    for (int j = fnd; j < users[idx].file_count-1; j++)
                        users[idx].files[j] = users[idx].files[j+1];
                    users[idx].file_count--;
                    code = 0;
                }
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // LIST_CONTENT <user_name>
        else if (argc == 2 && strcmp(args[0], "LIST_CONTENT") == 0) {
            pthread_mutex_lock(&server_mutex);
            int idx_req = -1, idx_tgt = -1;
            for (int i = 0; i < user_count; i++) {
                if (!strcmp(users[i].name, current))    idx_req = i;
                if (!strcmp(users[i].name, args[1]))    idx_tgt = i;
            }
            if (idx_req < 0) code = 1;
            else if (!users[idx_req].connected) code = 2;
            else if (idx_tgt < 0) code = 3;
            else {
                code = 0;
                int n = users[idx_tgt].file_count;
                send(fd, (char[]){0}, 1, 0);
                char tmp[32];
                snprintf(tmp, sizeof tmp, "%d", n);
                send(fd, tmp, strlen(tmp)+1, 0);
                for (int j = 0; j < n; j++) {
                    send(fd, users[idx_tgt].files[j].path,
                         strlen(users[idx_tgt].files[j].path)+1, 0);
                }
                pthread_mutex_unlock(&server_mutex);
                continue;
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // GET_FILE <user> <remote> <local>
        else if (argc == 4 && strcmp(args[0], "GET_FILE") == 0) {
            code = 0;  // Parte 1 no transfiere realmente
        }
        // DISCONNECT
        else if (argc == 1 && strcmp(args[0], "DISCONNECT") == 0) {
            pthread_mutex_lock(&server_mutex);
            for (int i = 0; i < user_count; i++) {
                if (!strcmp(users[i].name, current)) {
                    users[i].connected = 0;
                    break;
                }
            }
            pthread_mutex_unlock(&server_mutex);
            session = 0;
            code = 0;
        }

        // Enviar código de respuesta
        send(fd, (char[]){(char)code}, 1, 0);
    }

    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = SERVER_PORT, opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        if (opt == 'p') port = atoi(optarg);
        else {
            fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    int sfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd < 0) { perror("socket"); exit(EXIT_FAILURE); }

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(port)
    };
    if (bind(sfd, (struct sockaddr*)&addr, sizeof addr) < 0) {
        perror("bind"); exit(EXIT_FAILURE);
    }
    if (listen(sfd, BACKLOG) < 0) {
        perror("listen"); exit(EXIT_FAILURE);
    }

    printf("s > init server 0.0.0.0:%d\ns > ", port);
    fflush(stdout);

    while (1) {
        int *cfd = malloc(sizeof *cfd);
        *cfd = accept(sfd, NULL, NULL);
        if (*cfd < 0) {
            perror("accept");
            free(cfd);
            continue;
        }
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, cfd);
        pthread_detach(tid);
    }

    close(sfd);
    return 0;
}
