#include "tuplas.h"
#include <tirpc/rpc/rpc.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_KEY_SIZE 256
#define MAX_VALUE_SIZE 256

/* Función auxiliar: obtiene el handle RPC utilizando la variable de entorno IP_TUPLAS */
static CLIENT* get_client() {
    char *server_ip = getenv("IP_TUPLAS");
    if (!server_ip) {
        fprintf(stderr, "Error: La variable de entorno IP_TUPLAS no está definida.\n");
        exit(1);
    }
    CLIENT *clnt = clnt_create(server_ip, TUPLAS_PROG, TUPLAS_VERS, "udp");
    if (clnt == NULL) {
        clnt_pcreateerror(server_ip);
        exit(1);
    }
    return clnt;
}

/* Proxy para destroy */
int proxy_destroy() {
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));  /* Para la estructura completa es aceptable */
    int *result = destroy_1(&req, clnt);
    if (result == NULL) {
        clnt_perror(clnt, "Error en la llamada destroy");
        clnt_destroy(clnt);
        return -1;
    }
    clnt_destroy(clnt);
    return *result;
}

/* Proxy para set_value */
int proxy_set_value(const char *key, const char *value) {
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    /* Usamos las macros definidas en lugar de sizeof(variable) */
    strncpy(req.key, key, MAX_KEY_SIZE - 1);
    req.key[MAX_KEY_SIZE - 1] = '\0';
    strncpy(req.value, value, MAX_VALUE_SIZE - 1);
    req.value[MAX_VALUE_SIZE - 1] = '\0';
    
    int *result = set_value_1(&req, clnt);
    if (result == NULL) {
        clnt_perror(clnt, "Error en la llamada set_value");
        clnt_destroy(clnt);
        return -1;
    }
    clnt_destroy(clnt);
    return *result;
}

/* Proxy para get_value */
int proxy_get_value(const char *key, char *out_value) {
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    strncpy(req.key, key, MAX_KEY_SIZE - 1);
    req.key[MAX_KEY_SIZE - 1] = '\0';
    
    response *res = get_value_1(&req, clnt);
    if (res == NULL) {
        clnt_perror(clnt, "Error en la llamada get_value");
        clnt_destroy(clnt);
        return -1;
    }
    if (res->status == 0) {
        strncpy(out_value, res->value, MAX_VALUE_SIZE - 1);
        out_value[MAX_VALUE_SIZE - 1] = '\0';
    }
    clnt_destroy(clnt);
    return res->status;
}

/* Proxy para delete_key */
int proxy_delete_key(const char *key) {
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    strncpy(req.key, key, MAX_KEY_SIZE - 1);
    req.key[MAX_KEY_SIZE - 1] = '\0';
    
    int *result = delete_key_1(&req, clnt);
    if (result == NULL) {
        clnt_perror(clnt, "Error en la llamada delete_key");
        clnt_destroy(clnt);
        return -1;
    }
    clnt_destroy(clnt);
    return *result;
}

/* Proxy para modify_value */
int proxy_modify_value(const char *key, const char *new_value) {
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    strncpy(req.key, key, MAX_KEY_SIZE - 1);
    req.key[MAX_KEY_SIZE - 1] = '\0';
    strncpy(req.value, new_value, MAX_VALUE_SIZE - 1);
    req.value[MAX_VALUE_SIZE - 1] = '\0';
    
    int *result = modify_value_1(&req, clnt);
    if (result == NULL) {
        clnt_perror(clnt, "Error en la llamada modify_value");
        clnt_destroy(clnt);
        return -1;
    }
    clnt_destroy(clnt);
    return *result;
}

/* Proxy para exist */
int proxy_exist(const char *key) {
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    strncpy(req.key, key, MAX_KEY_SIZE - 1);
    req.key[MAX_KEY_SIZE - 1] = '\0';
    
    int *result = exist_1(&req, clnt);
    if (result == NULL) {
        clnt_perror(clnt, "Error en la llamada exist");
        clnt_destroy(clnt);
        return -1;
    }
    clnt_destroy(clnt);
    return *result;
}
