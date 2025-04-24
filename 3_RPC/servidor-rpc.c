/* servidor-rpc.c */

#include "tuplas.h"
#include "claves.h"
#include <tirpc/rpc/rpc.h>
#include <stdlib.h>
#include <string.h>

static int     int_res;    /* espacio para devolver int* */
static response resp_res;  /* espacio para devolver response* */

/* 1) DESTROY */
int *
destroy_1_svc(request req, struct svc_req *rqstp)
{
    (void)rqstp;
    int_res = destroy();
    return &int_res;
}

/* 2) SET_VALUE */
int *
set_value_1_svc(request req, struct svc_req *rqstp)
{
    struct Coord c;
    (void)rqstp;

    c.x = req.value3.x;
    c.y = req.value3.y;

    int_res = set_value(
        req.key,
        req.value1,
        req.N_value2,
        req.V_value2.V_value2_val,
        c
    );
    return &int_res;
}

/* 3) GET_VALUE */
response *
get_value_1_svc(request req, struct svc_req *rqstp)
{
    char        buf1[256];
    int         n2;
    double      v2[256];
    struct Coord c;
    (void)rqstp;

    /* Llamada interna */
    resp_res.status = get_value(
        req.key,
        buf1,
        &n2,
        v2,
        &c
    );

    if (resp_res.status == 0) {
        /* cadena */
        resp_res.value1 = strdup(buf1);
        /* vector dinámico */
        resp_res.N_value2 = n2;
        resp_res.V_value2.V_value2_len = n2;
        resp_res.V_value2.V_value2_val = malloc(n2 * sizeof(double));
        memcpy(resp_res.V_value2.V_value2_val, v2, n2 * sizeof(double));
        /* coordenadas */
        resp_res.value3.x = c.x;
        resp_res.value3.y = c.y;
    } else {
        /* en error, devolvemos estructura válida pero vacía */
        resp_res.value1 = strdup("");
        resp_res.N_value2 = 0;
        resp_res.V_value2.V_value2_len = 0;
        resp_res.V_value2.V_value2_val = NULL;
        resp_res.value3.x = 0;
        resp_res.value3.y = 0;
    }

    return &resp_res;
}

/* 4) DELETE_KEY */
int *
delete_key_1_svc(request req, struct svc_req *rqstp)
{
    (void)rqstp;
    int_res = delete_key(req.key);
    return &int_res;
}

/* 5) MODIFY_VALUE */
int *
modify_value_1_svc(request req, struct svc_req *rqstp)
{
    struct Coord c;
    (void)rqstp;

    c.x = req.value3.x;
    c.y = req.value3.y;

    int_res = modify_value(
        req.key,
        req.value1,
        req.N_value2,
        req.V_value2.V_value2_val,
        c
    );
    return &int_res;
}

/* 6) EXIST */
int *
exist_1_svc(request req, struct svc_req *rqstp)
{
    (void)rqstp;
    int_res = exist(req.key);
    return &int_res;
}
