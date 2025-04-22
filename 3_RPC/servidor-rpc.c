#include "tuplas.h"
#include "claves.h"
#include <stdio.h>
#include <string.h>

/* La función destroy_1_svc invoca la función destroy implementada en claves.c */
int * destroy_1_svc(request *req, struct svc_req *rqstp) {
    static int result;
    result = destroy();
    return &result;
}

/* La función set_value_1_svc invoca a set_value pasándole key y value */
int * set_value_1_svc(request *req, struct svc_req *rqstp) {
    static int result;
    result = set_value(req->key, req->value);
    return &result;
}

/* La función get_value_1_svc obtiene el valor asociado a una clave */
response * get_value_1_svc(request *req, struct svc_req *rqstp) {
    static response res;
    char *val = get_value(req->key);
    
    if (val != NULL) {
        res.status = 0;
        /* Se copia el valor de forma segura utilizando strncpy */
        strncpy(res.value, val, sizeof(res.value) - 1);
        res.value[sizeof(res.value) - 1] = '\0';
    } else {
        res.status = 1;  /* Error o clave no encontrada */
        res.value[0] = '\0';
    }
    return &res;
}

/* La función delete_key_1_svc elimina la clave indicada */
int * delete_key_1_svc(request *req, struct svc_req *rqstp) {
    static int result;
    result = delete_key(req->key);
    return &result;
}

/* La función modify_value_1_svc modifica el valor asociado a una clave */
int * modify_value_1_svc(request *req, struct svc_req *rqstp) {
    static int result;
    result = modify_value(req->key, req->value);
    return &result;
}

/* La función exist_1_svc verifica si existe una clave */
int * exist_1_svc(request *req, struct svc_req *rqstp) {
    static int result;
    result = exist(req->key);
    return &result;
}
