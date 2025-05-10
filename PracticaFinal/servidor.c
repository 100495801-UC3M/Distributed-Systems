// servidor.c (Corregido para Parte 3 - Cliente RPC y errores de compilación)

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
#include <signal.h>

#include "logger.h" 

#define SERVER_PORT       8080
#define BACKLOG           10
#define MSG_BUFFER_SIZE   2048
#define MAX_USERS             100
#define MAX_FILES_PER_USER    100
#define MAX_NAME_LEN          64
#define MAX_PATH_LEN         256
#define MAX_DESC_LEN         256

typedef struct {
    char path[MAX_PATH_LEN];
    char desc[MAX_DESC_LEN];
} file_t;

typedef struct {
    char name[MAX_NAME_LEN];
    char ip[INET_ADDRSTRLEN];
    int  port;
    int  connected;
    int  file_count;
    file_t files[MAX_FILES_PER_USER];
    int  socket_fd;
} user_t;

static user_t users[MAX_USERS];
static int    user_count = 0;
static pthread_mutex_t server_mutex = PTHREAD_MUTEX_INITIALIZER;
volatile sig_atomic_t keep_running = 1;
int sfd_global = -1;

CLIENT *rpc_logger_client = NULL;
char *rpc_logger_server_host = NULL;

void safe_strncpy(char *dest, const char *src, size_t n) {
    if (n == 0) return;
    strncpy(dest, src, n - 1);
    dest[n - 1] = '\0';
}

static int parse(char *buf, int buflen, char **args, int max_args) {
    int argc = 0;
    char *p = buf;
    while ((p < buf + buflen) && (argc < max_args)) {
        args[argc++] = p;
        while (*p != '\0' && (p < buf + buflen -1) ) {
            p++;
        }
        if (*p == '\0') {
             p++; 
        } else { 
            break; 
        }
    }
    return argc;
}

void signal_handler(int sig) {
    if (sig == SIGINT) {
        printf("\nSIGINT received, shutting down server...\ns> ");
        fflush(stdout);
        keep_running = 0; 
        if(sfd_global != -1) {
             shutdown(sfd_global, SHUT_RDWR); 
             close(sfd_global); 
             sfd_global = -1; 
        }
    }
}

void *handle_client(void *arg) {
    int fd = *(int*)arg;
    free(arg);

    char buf[MSG_BUFFER_SIZE];
    char *args[10]; 
    char current_user_name[MAX_NAME_LEN] = "";
    int session_active = 1;
    unsigned char response_code_p2p; // Renombrada para claridad
    char *received_timestamp_str = NULL;

    while (session_active && keep_running) {
        ssize_t bytes_received = recv(fd, buf, MSG_BUFFER_SIZE -1, 0);
        if (bytes_received <= 0) {
            break; 
        }
        buf[bytes_received] = '\0'; 

        int argc = parse(buf, bytes_received, args, 10);
        fflush(stdout);

        if (argc == 0) continue;

        response_code_p2p = 4; // Código de error genérico P2P por defecto

        if (argc > 1) { // Asignar timestamp si el comando lo lleva
            if (strcmp(args[0], "REGISTER") == 0 || strcmp(args[0], "UNREGISTER") == 0 ||
                strcmp(args[0], "CONNECT") == 0 || strcmp(args[0], "PUBLISH") == 0 ||
                strcmp(args[0], "DELETE") == 0 || strcmp(args[0], "LIST_USERS") == 0 ||
                strcmp(args[0], "LIST_CONTENT") == 0 || strcmp(args[0], "DISCONNECT") == 0) {
                received_timestamp_str = args[1];
            } else {
                received_timestamp_str = (char *)"TS_N/A_CMD_UNKNOWN"; 
            }
        } else {
            received_timestamp_str = (char *)"TS_N/A_NO_TS_ARG"; 
        }

        // Log de la operación P2P en consola del servidor
        printf("s> OPERATION %s", args[0]);
        if (current_user_name[0] != '\0') {
            printf(" FROM %s", current_user_name);
        } else if (argc > 2 && (strcmp(args[0], "REGISTER") == 0 || strcmp(args[0], "UNREGISTER") == 0 || strcmp(args[0], "CONNECT") == 0)) {
            if (args[2] != NULL && strlen(args[2]) > 0) printf(" FROM %s", args[2]);
        }
        printf("\n"); 
        fflush(stdout);

        // --- INICIO LÓGICA DE COMANDOS P2P ---
        // REGISTER <timestamp> <user>
        if (argc == 3 && strcmp(args[0], "REGISTER") == 0) {
            char *user_to_register = args[2];
            pthread_mutex_lock(&server_mutex);
            int found = 0;
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].name, user_to_register) == 0) { found = 1; break; }
            }
            if (found) {
                response_code_p2p = 1;
            } else if (user_count < MAX_USERS) {
                safe_strncpy(users[user_count].name, user_to_register, MAX_NAME_LEN);
                users[user_count].connected = 0;
                users[user_count].file_count = 0;
                users[user_count].socket_fd = -1;
                user_count++;
                response_code_p2p = 0;
            } else {
                response_code_p2p = 2;
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // UNREGISTER <timestamp> <user>
        else if (argc == 3 && strcmp(args[0], "UNREGISTER") == 0) {
            char *user_to_unregister = args[2];
            pthread_mutex_lock(&server_mutex);
            int idx = -1;
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].name, user_to_unregister) == 0) { idx = i; break; }
            }
            if (idx < 0) {
                response_code_p2p = 1;
            } else {
                if(users[idx].connected && users[idx].socket_fd == fd) {
                    users[idx].connected = 0;
                    users[idx].socket_fd = -1;
                }
                for (int j = idx; j < user_count - 1; j++) {
                    users[j] = users[j+1];
                }
                user_count--;
                response_code_p2p = 0;
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // CONNECT <timestamp> <user> <port_str>
        else if (argc == 4 && strcmp(args[0], "CONNECT") == 0) {
            char *user_to_connect = args[2];
            int   port_from_client = atoi(args[3]);
            struct sockaddr_in sa; socklen_t len = sizeof sa;
            char peer_ip[INET_ADDRSTRLEN];

            if (getpeername(fd, (struct sockaddr*)&sa, &len) == 0) {
                inet_ntop(AF_INET, &sa.sin_addr, peer_ip, INET_ADDRSTRLEN);
            } else {
                safe_strncpy(peer_ip, "UNKNOWN_IP", INET_ADDRSTRLEN);
            }

            pthread_mutex_lock(&server_mutex);
            int idx = -1;
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].name, user_to_connect) == 0) { idx = i; break; }
            }
            if (idx < 0) {
                response_code_p2p = 1;
            } else if (users[idx].connected) {
                response_code_p2p = 2;
            } else {
                users[idx].connected = 1;
                safe_strncpy(users[idx].ip, peer_ip, INET_ADDRSTRLEN);
                users[idx].port = port_from_client;
                users[idx].socket_fd = fd;
                safe_strncpy(current_user_name, user_to_connect, MAX_NAME_LEN);
                response_code_p2p = 0;
            }
            pthread_mutex_unlock(&server_mutex);
        }
        else if (current_user_name[0] == '\0') { // Si no es REGISTER/UNREGISTER/CONNECT y no hay current_user_name
             response_code_p2p = 2; 
        }
        // PUBLISH <timestamp> <user_name_publica> <file_name> <description>
        else if (argc == 5 && strcmp(args[0], "PUBLISH") == 0 && strcmp(current_user_name, args[2]) == 0) {
            char *file_name_to_publish = args[3];
            char *description = args[4];
            pthread_mutex_lock(&server_mutex);
            int idx = -1; 
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].name, current_user_name) == 0) { idx = i; break; }
            }
            if (idx <0 || !users[idx].connected) { 
                response_code_p2p = 2; 
            } else {
                int file_exists = 0;
                for (int j = 0; j < users[idx].file_count; j++) {
                    if (strcmp(users[idx].files[j].path, file_name_to_publish) == 0) {
                        file_exists = 1; break;
                    }
                }
                if (file_exists) {
                    response_code_p2p = 3; 
                } else if (users[idx].file_count < MAX_FILES_PER_USER) {
                    safe_strncpy(users[idx].files[users[idx].file_count].path, file_name_to_publish, MAX_PATH_LEN);
                    safe_strncpy(users[idx].files[users[idx].file_count].desc, description, MAX_DESC_LEN);
                    users[idx].file_count++;
                    response_code_p2p = 0; 
                } else {
                    response_code_p2p = 4; 
                }
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // DELETE <timestamp> <user_name_borra> <file_name>
        else if (argc == 4 && strcmp(args[0], "DELETE") == 0 && strcmp(current_user_name, args[2]) == 0) {
            char *file_name_to_delete = args[3];
            pthread_mutex_lock(&server_mutex);
            int idx = -1;
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].name, current_user_name) == 0) { idx = i; break; }
            }
            if (idx < 0 || !users[idx].connected) {
                response_code_p2p = 2; 
            } else {
                int file_idx_to_delete = -1;
                for (int j = 0; j < users[idx].file_count; j++) {
                    if (strcmp(users[idx].files[j].path, file_name_to_delete) == 0) {
                        file_idx_to_delete = j; break;
                    }
                }
                if (file_idx_to_delete < 0) {
                    response_code_p2p = 3; 
                } else {
                    for (int j = file_idx_to_delete; j < users[idx].file_count - 1; j++) {
                        users[idx].files[j] = users[idx].files[j+1];
                    }
                    users[idx].file_count--;
                    response_code_p2p = 0; 
                }
            }
            pthread_mutex_unlock(&server_mutex);
        }
        // LIST_USERS <timestamp> <user_name_solicitante>
        else if (argc == 3 && strcmp(args[0], "LIST_USERS") == 0 && strcmp(current_user_name, args[2]) == 0) {
            pthread_mutex_lock(&server_mutex);
            int requester_idx = -1;
            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].name, current_user_name) == 0) { requester_idx = i; break; }
            }

            if (requester_idx == -1) { // Solicitante no encontrado
                response_code_p2p = 1; // Código PDF: el usuario que realiza la operación no existe
            } else if (!users[requester_idx].connected) { // Solicitante no conectado
                response_code_p2p = 2;
            } else { // Solicitante OK y conectado
                response_code_p2p = 0; // Marcamos como OK para la lógica RPC, pero la respuesta al cliente P2P es diferente
                unsigned char ok_code_p2p = 0;
                if(send(fd, &ok_code_p2p, 1, 0) <=0) { session_active = 0; pthread_mutex_unlock(&server_mutex); continue; }

                int connected_users_count = 0;
                for (int i = 0; i < user_count; i++) {
                    if (users[i].connected) connected_users_count++;
                }
                char count_str[12];
                snprintf(count_str, sizeof count_str, "%d", connected_users_count);
                if(send(fd, count_str, strlen(count_str) + 1, 0) <=0) { session_active = 0; pthread_mutex_unlock(&server_mutex); continue; }

                for (int i = 0; i < user_count; i++) {
                    if (users[i].connected) {
                        if(send(fd, users[i].name, strlen(users[i].name) + 1, 0) <=0) { session_active = 0; break; }
                        if(send(fd, users[i].ip, strlen(users[i].ip) + 1, 0) <=0) { session_active = 0; break; }
                        char port_str_lu[12]; // Nombre diferente para evitar shadowing
                        snprintf(port_str_lu, sizeof port_str_lu, "%d", users[i].port);
                        if(send(fd, port_str_lu, strlen(port_str_lu) + 1, 0) <=0) { session_active = 0; break; }
                    }
                }
            }
            pthread_mutex_unlock(&server_mutex); 
            if (!session_active) continue; // Si un send falló
            
            // Llamada RPC para LIST_USERS (éxito o fallo según response_code_p2p)
            if (rpc_logger_client != NULL && received_timestamp_str != NULL && 
                strcmp(received_timestamp_str, "TS_N/A_CMD_UNKNOWN") != 0 && 
                strcmp(received_timestamp_str, "TS_N/A_NO_TS_ARG") != 0) {
                log_data rpc_log_input;
                char operation_detail_for_rpc[MAX_STR_LEN];
                safe_strncpy(operation_detail_for_rpc, args[0], MAX_STR_LEN); // "LIST_USERS"
                
                rpc_log_input.username = current_user_name; // Solicitante
                rpc_log_input.operation_details = operation_detail_for_rpc;
                rpc_log_input.timestamp_str = received_timestamp_str;
                
                struct timeval timeout_rpc_call = {2, 0};
                clnt_control(rpc_logger_client, CLSET_TIMEOUT, &timeout_rpc_call);
                if (log_operation_1(&rpc_log_input, rpc_logger_client) == NULL && response_code_p2p == 0) { // Solo loguear error RPC si la op P2P fue OK
                    fprintf(stderr, "s> ALERTA: Fallo la llamada RPC de logging para %s (Host: %s)\n", args[0], rpc_logger_server_host ? rpc_logger_server_host : "N/A"); fflush(stderr);
                }
            }
            if (response_code_p2p == 0) continue; // Si fue exitoso y se enviaron datos, no enviar código de respuesta P2P genérico.
        }
        // LIST_CONTENT <timestamp> <user_name_solicitante> <user_name_contenido>
        else if (argc == 4 && strcmp(args[0], "LIST_CONTENT") == 0 && strcmp(current_user_name, args[2]) == 0) {
            char *target_user_name_content = args[3];
            pthread_mutex_lock(&server_mutex);
            int requester_idx = -1, target_idx = -1;

            for (int i = 0; i < user_count; i++) {
                if (strcmp(users[i].name, current_user_name) == 0) requester_idx = i;
                if (strcmp(users[i].name, target_user_name_content) == 0) target_idx = i;
            }

            if (requester_idx < 0) { 
                response_code_p2p = 1; 
            } else if (!users[requester_idx].connected) { 
                response_code_p2p = 2; 
            } else if (target_idx < 0) { 
                response_code_p2p = 3; 
            } else {
                response_code_p2p = 0; // OK para lógica RPC, la respuesta P2P es diferente
                unsigned char ok_code_p2p = 0;
                if(send(fd, &ok_code_p2p, 1, 0) <=0) { session_active = 0; pthread_mutex_unlock(&server_mutex); continue; }
                
                int files_to_list_count = users[target_idx].file_count;
                char count_str_lc[12]; // Nombre diferente
                snprintf(count_str_lc, sizeof count_str_lc, "%d", files_to_list_count);
                if(send(fd, count_str_lc, strlen(count_str_lc) + 1, 0) <=0) { session_active = 0; pthread_mutex_unlock(&server_mutex); continue; }

                for (int j = 0; j < files_to_list_count; j++) {
                    if(send(fd, users[target_idx].files[j].path, strlen(users[target_idx].files[j].path) + 1, 0) <=0) { session_active = 0; break;}
                }
            }
            pthread_mutex_unlock(&server_mutex);
            if (!session_active) continue;

            if (rpc_logger_client != NULL && received_timestamp_str != NULL ) {
                log_data rpc_log_input_lc; // Renombrada
                char op_detail_lc[MAX_STR_LEN];
                snprintf(op_detail_lc, MAX_STR_LEN, "%s %s", args[0], target_user_name_content); 
                op_detail_lc[MAX_STR_LEN-1] = '\0';

                rpc_log_input_lc.username = current_user_name; // Solicitante
                rpc_log_input_lc.operation_details = op_detail_lc;
                rpc_log_input_lc.timestamp_str = received_timestamp_str;
                
                struct timeval timeout_rpc_lc = {2,0};
                clnt_control(rpc_logger_client, CLSET_TIMEOUT, &timeout_rpc_lc);
                if(log_operation_1(&rpc_log_input_lc, rpc_logger_client) == NULL && response_code_p2p == 0){ // Solo log error RPC si op P2P fue OK
                     fprintf(stderr, "s> ALERTA: Fallo la llamada RPC de logging para %s (Host: %s)\n", op_detail_lc, rpc_logger_server_host ? rpc_logger_server_host : "N/A"); fflush(stderr);
                }
            }
            if (response_code_p2p == 0) continue; // Si fue exitoso y se enviaron datos, no enviar código genérico
        }
        // DISCONNECT <timestamp> <user_name_desconecta>
        else if (argc == 3 && strcmp(args[0], "DISCONNECT") == 0) {
            char *user_to_disconnect = args[2];
            if (current_user_name[0] != '\0' && strcmp(current_user_name, user_to_disconnect) == 0) {
                pthread_mutex_lock(&server_mutex);
                int idx = -1;
                for (int i = 0; i < user_count; i++) {
                    if (strcmp(users[i].name, current_user_name) == 0) { idx = i; break; }
                }
                if (idx != -1 && users[idx].connected) {
                    users[idx].connected = 0;
                    users[idx].socket_fd = -1; 
                    response_code_p2p = 0; 
                } else if (idx != -1 && !users[idx].connected) {
                    response_code_p2p = 2;
                } else { 
                    response_code_p2p = 1;
                }
                pthread_mutex_unlock(&server_mutex);
                session_active = 0; 
            } else if (current_user_name[0] == '\0') { 
                response_code_p2p = 2; 
            } else { 
                response_code_p2p = 3; 
                session_active = 0; 
            }
        }
        else {
            // Comando desconocido o argc incorrecto para el comando
            // response_code_p2p ya es 4 (error genérico P2P)
            fflush(stdout);
        }
        // --- FIN LÓGICA DE COMANDOS P2P ---

        // ---- INICIO DE LA LLAMADA RPC (para comandos que NO usaron 'continue') ----
        // Esto es para REGISTER, UNREGISTER, CONNECT, PUBLISH, DELETE, DISCONNECT,
        // y para LIST_USERS/LIST_CONTENT si tuvieron un error P2P.
        if (!((response_code_p2p == 0) && (strcmp(args[0], "LIST_USERS") == 0 || strcmp(args[0], "LIST_CONTENT") == 0))) {
            if (rpc_logger_client != NULL && received_timestamp_str != NULL && 
                strcmp(received_timestamp_str, "TS_N/A_CMD_UNKNOWN") != 0 && 
                strcmp(received_timestamp_str, "TS_N/A_NO_TS_ARG") != 0) {
                
                log_data rpc_log_input_generic;
                char operation_detail_for_rpc_generic[MAX_STR_LEN];
                char *user_for_rpc_generic = NULL;

                // Determinar usuario para RPC
                if (current_user_name[0] != '\0') {
                    user_for_rpc_generic = current_user_name;
                } else if (argc > 2 && (strcmp(args[0], "REGISTER") == 0 || strcmp(args[0], "UNREGISTER") == 0 || strcmp(args[0], "CONNECT") == 0)) {
                    user_for_rpc_generic = args[2]; // Usuario es args[2] (CMD, TS, USER)
                } else {
                    user_for_rpc_generic = (char *)"ANONYMOUS_GEN"; // Fallback
                }
                
                // Construir detalle de operación para RPC
                // PUBLISH <ts> <user> <file> <desc> -> argc=5, file es args[3]
                if ((strcmp(args[0], "PUBLISH") == 0) && argc >= 4) { // Mínimo CMD, TS, USER, FILE
                    snprintf(operation_detail_for_rpc_generic, MAX_STR_LEN, "%s %s", args[0], args[3]);
                } 
                // DELETE <ts> <user> <file> -> argc=4, file es args[3]
                else if ((strcmp(args[0], "DELETE") == 0) && argc >= 4) {
                    snprintf(operation_detail_for_rpc_generic, MAX_STR_LEN, "%s %s", args[0], args[3]);
                }
                // LIST_CONTENT <ts> <user_req> <user_target> -> argc=4, user_target es args[3]
                // Esta ya se loguea arriba si es exitosa. Si es un error, logueamos solo el comando.
                // Solo necesitamos el nombre del comando para otros casos.
                else {
                    safe_strncpy(operation_detail_for_rpc_generic, args[0], MAX_STR_LEN);
                }
                operation_detail_for_rpc_generic[MAX_STR_LEN-1] = '\0';

                rpc_log_input_generic.username = user_for_rpc_generic;
                rpc_log_input_generic.operation_details = operation_detail_for_rpc_generic;
                rpc_log_input_generic.timestamp_str = received_timestamp_str; // args[1]
                
                        rpc_log_input_generic.username, 
                        rpc_log_input_generic.operation_details, 
                        rpc_log_input_generic.timestamp_str);
                fflush(stdout);

                struct timeval timeout_rpc_call = {2, 0}; 
                clnt_control(rpc_logger_client, CLSET_TIMEOUT, &timeout_rpc_call);
                if (log_operation_1(&rpc_log_input_generic, rpc_logger_client) == NULL) {
                    fprintf(stderr, "s> ALERTA: Fallo la llamada RPC de logging para OP: %s USER: %s (Host: %s)\n", 
                            operation_detail_for_rpc_generic, 
                            user_for_rpc_generic,
                            rpc_logger_server_host ? rpc_logger_server_host : "N/A"); 
                    fflush(stderr);
                }
            }
        }
        // ---- FIN DE LA LLAMADA RPC ----

        // Enviar código de respuesta P2P al cliente (si no se usó 'continue' para LIST_USERS/LIST_CONTENT exitosos)
        if (!((response_code_p2p == 0) && (strcmp(args[0], "LIST_USERS") == 0 || strcmp(args[0], "LIST_CONTENT") == 0))) {
            unsigned char final_code_byte = (unsigned char)response_code_p2p;
            if (send(fd, &final_code_byte, 1, 0) <= 0) {
                session_active = 0; 
            }
        }
        
    } 

    if (current_user_name[0] != '\0') {
        pthread_mutex_lock(&server_mutex);
        for (int i = 0; i < user_count; i++) {
            if (strcmp(users[i].name, current_user_name) == 0) {
                if (users[i].socket_fd == fd) { 
                    users[i].connected = 0;
                    users[i].socket_fd = -1;
                    printf("s> User %s disconnected (P2P session closed)\n", current_user_name);
                    fflush(stdout);

                    // Log RPC para DISCONNECT implícito si la sesión se cierra inesperadamente
                    if (rpc_logger_client != NULL && strcmp(args[0], "DISCONNECT") != 0) { // No loguear si ya fue un DISCONNECT explícito
                         if (received_timestamp_str != NULL && strcmp(received_timestamp_str, "TS_N/A_CMD_UNKNOWN") != 0 && strcmp(received_timestamp_str, "TS_N/A_NO_TS_ARG") != 0) {
                            log_data rpc_log_input_dc;
                            char op_detail_dc[MAX_STR_LEN];
                            safe_strncpy(op_detail_dc, "IMPLICIT_DISCONNECT", MAX_STR_LEN);
                            
                            rpc_log_input_dc.username = current_user_name;
                            rpc_log_input_dc.operation_details = op_detail_dc;
                            // Usar un timestamp "ahora" sería ideal, pero no tenemos acceso fácil aquí.
                            // Usamos el último timestamp recibido si está disponible, o uno genérico.
                            char* ts_for_implicit_dc = (received_timestamp_str && strncmp(received_timestamp_str, "TS_N/A", 6) != 0) ? 
                                                        received_timestamp_str : (char*)"NO_TS_FOR_IMPLICIT_DC";
                            rpc_log_input_dc.timestamp_str = ts_for_implicit_dc;

                            struct timeval timeout_rpc_dc = {2,0};
                            clnt_control(rpc_logger_client, CLSET_TIMEOUT, &timeout_rpc_dc);
                            if(log_operation_1(&rpc_log_input_dc, rpc_logger_client) == NULL){
                                 fprintf(stderr, "s> ALERTA: Fallo la llamada RPC de logging para IMPLICIT_DISCONNECT (Host: %s)\n", rpc_logger_server_host ? rpc_logger_server_host : "N/A"); fflush(stderr);
                            }
                        }
                    }
                }
                break;
            }
        }
        pthread_mutex_unlock(&server_mutex);
    }
    close(fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    int opt;
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        if (opt == 'p') {
            port = atoi(optarg);
        } else {
            fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    signal(SIGINT, signal_handler); 

    rpc_logger_server_host = getenv("LOG_RPC_IP");
    if (rpc_logger_server_host == NULL) {
        fprintf(stderr, "s> ADVERTENCIA: Variable de entorno LOG_RPC_IP no definida. El logging RPC estará deshabilitado.\n");
        rpc_logger_client = NULL; 
    } else {
        rpc_logger_client = clnt_create(rpc_logger_server_host, LOGGER_PROG, LOGGER_VERS, "udp");
        if (rpc_logger_client == NULL) {
            clnt_pcreateerror(rpc_logger_server_host);
            fprintf(stderr, "s> ADVERTENCIA: No se pudo crear el cliente RPC para %s. El logging RPC estará deshabilitado.\n", rpc_logger_server_host);
        } else {
            printf("s> Cliente RPC de logging intentará conectar al host %s\n", rpc_logger_server_host);
        }
    }
    fflush(stdout); 
    fflush(stderr);

    sfd_global = socket(AF_INET, SOCK_STREAM, 0);
    if (sfd_global < 0) { perror("socket"); exit(EXIT_FAILURE); }
    int reuse = 1;
    if (setsockopt(sfd_global, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(sfd_global); 
        exit(EXIT_FAILURE);
    }
    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_addr.s_addr = INADDR_ANY,
        .sin_port        = htons(port)
    };
    if (bind(sfd_global, (struct sockaddr*)&addr, sizeof addr) < 0) {
        perror("bind"); close(sfd_global); exit(EXIT_FAILURE);
    }
    if (listen(sfd_global, BACKLOG) < 0) {
        perror("listen"); close(sfd_global); exit(EXIT_FAILURE);
    }

    printf("s> init server 0.0.0.0:%d (P2P)\n", port);
    fflush(stdout);

    while (keep_running) {
        int *cfd_ptr = malloc(sizeof(int));
        if (!cfd_ptr) { perror("malloc for client fd"); continue; }

        *cfd_ptr = accept(sfd_global, NULL, NULL);
        if (*cfd_ptr < 0) {
            free(cfd_ptr);
            if (errno == EINTR && !keep_running) {
                 printf("s> Server P2P accept-loop interrupted, shutting down.\n"); fflush(stdout);
                break;
            }
            if(keep_running && errno != EINTR) { 
                 perror("accept"); 
            }
            continue;
        }
        
        pthread_t tid;
        if (pthread_create(&tid, NULL, handle_client, cfd_ptr) != 0) {
            perror("pthread_create");
            free(cfd_ptr);
            close(*cfd_ptr); 
        } else {
            pthread_detach(tid); 
        }
    }

    printf("s> Servidor P2P principal apagándose...\n");
    fflush(stdout);
    
    if (sfd_global != -1) { 
        close(sfd_global);
        sfd_global = -1;
    }

    if (rpc_logger_client != NULL) {
        clnt_destroy(rpc_logger_client);
        rpc_logger_client = NULL;
        printf("s> Cliente RPC de logging desconectado.\n");
        fflush(stdout);
    }
    
    pthread_mutex_destroy(&server_mutex);
    printf("s> Servidor P2P principal apagado completamente.\n");
    fflush(stdout);
    return 0;
}