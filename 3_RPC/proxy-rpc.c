/* proxy-rpc.c */

#include <tirpc/rpc/rpc.h>
#include "tuplas.h"
#include "claves.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAX_VALUE2_LEN 32
#define MAX_STR_LEN    256

/** init_proxy: inicializa IP_TUPLAS con host o "host:puerto" */
void
init_proxy(const char *host)
{
    if (setenv("IP_TUPLAS", host, 1) != 0) {
        perror("init_proxy: setenv IP_TUPLAS");
        exit(EXIT_FAILURE);
    }
}

/** get_client: crea y devuelve CLIENT* según IP_TUPLAS */
static CLIENT *
get_client(void)
{
    char *server = getenv("IP_TUPLAS");
    if (!server) {
        fprintf(stderr, "Error: IP_TUPLAS no definida\n");
        exit(EXIT_FAILURE);
    }
    CLIENT *clnt = clnt_create(server, TUPLAS_PROG, TUPLAS_VERS, "udp");
    if (!clnt) {
        clnt_pcreateerror("clnt_create");
        exit(EXIT_FAILURE);
    }
    return clnt;
}

/** destroy(): RPC DESTROY */
int
destroy(void)
{
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    req.value1                = "";
    req.V_value2.V_value2_len = 0;
    req.V_value2.V_value2_val = NULL;
    req.value3.x              = 0;
    req.value3.y              = 0;

    int *rp = destroy_1(req, clnt);
    if (!rp) {
        clnt_perror(clnt, "Error RPC destroy");
        clnt_destroy(clnt);
        return -1;
    }
    int status = *rp;
    clnt_destroy(clnt);
    return status;
}

/** exist(key): RPC EXIST */
int
exist(int key)
{
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    req.key                   = key;
    req.value1                = "";
    req.V_value2.V_value2_len = 0;
    req.V_value2.V_value2_val = NULL;
    req.value3.x              = 0;
    req.value3.y              = 0;

    int *rp = exist_1(req, clnt);
    if (!rp) {
        clnt_perror(clnt, "Error RPC exist");
        clnt_destroy(clnt);
        return -1;
    }
    int status = *rp;
    clnt_destroy(clnt);
    return status;
}

/** set_value(key, value1, N_value2, V_value2, coord): RPC SET_VALUE */
int
set_value(int key,
          char *value1,
          int N_value2,
          double *V_value2,
          struct Coord coord)
{
    /* 1) Duplicado? */
    int e = exist(key);
    if (e < 0) return -1;
    if (e == 1) return -1;

    /* 2) Rango vector */
    if (N_value2 < 1 || N_value2 > MAX_VALUE2_LEN) return -1;

    /* 3) Rango string */
    if ((int)strlen(value1) >= MAX_STR_LEN) return -1;

    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));

    req.key          = key;
    req.value1       = value1;

    /* aquí estaba la omisión: ¡asignamos N_value2 antes del stub! */
    req.N_value2                      = N_value2;
    req.V_value2.V_value2_len        = N_value2;
    req.V_value2.V_value2_val        = malloc(N_value2 * sizeof(double));
    memcpy(req.V_value2.V_value2_val, V_value2, N_value2 * sizeof(double));

    RPC_Coord rpc_c = { .x = coord.x, .y = coord.y };
    req.value3 = rpc_c;

    int *rp = set_value_1(req, clnt);
    free(req.V_value2.V_value2_val);
    if (!rp) {
        clnt_perror(clnt, "Error RPC set_value");
        clnt_destroy(clnt);
        return -1;
    }
    int status = *rp;
    clnt_destroy(clnt);
    return status;
}

/** get_value(key, out...): RPC GET_VALUE */
int
get_value(int key,
          char *value1,
          int *N_value2,
          double *V_value2,
          struct Coord *coord)
{
    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    req.key                   = key;
    req.value1                = "";
    req.N_value2              = 0;
    req.V_value2.V_value2_len = 0;
    req.V_value2.V_value2_val = NULL;
    req.value3.x              = 0;
    req.value3.y              = 0;

    response *resp = get_value_1(req, clnt);
    if (!resp) {
        clnt_perror(clnt, "Error RPC get_value");
        clnt_destroy(clnt);
        return -1;
    }
    int status = resp->status;
    if (status == 0) {
        strncpy(value1, resp->value1, MAX_STR_LEN - 1);
        value1[MAX_STR_LEN - 1] = '\0';

        int len = resp->V_value2.V_value2_len;
        if (len > MAX_VALUE2_LEN) len = MAX_VALUE2_LEN;
        *N_value2 = len;
        for (int i = 0; i < len; ++i)
            V_value2[i] = resp->V_value2.V_value2_val[i];

        coord->x = resp->value3.x;
        coord->y = resp->value3.y;
    }
    clnt_destroy(clnt);
    return status;
}

/** delete_key(key): RPC DELETE_KEY */
int
delete_key(int key)
{
    int e = exist(key);
    if (e < 0) return -1;
    if (e == 0) return -1;

    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));
    req.key                   = key;
    req.value1                = "";
    req.N_value2              = 0;
    req.V_value2.V_value2_len = 0;
    req.V_value2.V_value2_val = NULL;
    req.value3.x              = 0;
    req.value3.y              = 0;

    int *rp = delete_key_1(req, clnt);
    if (!rp) {
        clnt_perror(clnt, "Error RPC delete_key");
        clnt_destroy(clnt);
        return -1;
    }
    int status = *rp;
    clnt_destroy(clnt);
    return status;
}

/** modify_value(key, ...): RPC MODIFY_VALUE */
int
modify_value(int key,
             char *value1,
             int N_value2,
             double *V_value2,
             struct Coord coord)
{
    int e = exist(key);
    if (e < 0) return -1;
    if (e == 0) return -1;
    if (N_value2 < 1 || N_value2 > MAX_VALUE2_LEN) return -1;
    if ((int)strlen(value1) >= MAX_STR_LEN) return -1;

    CLIENT *clnt = get_client();
    request req;
    memset(&req, 0, sizeof(req));

    req.key          = key;
    req.value1       = value1;
    req.N_value2     = N_value2;
    req.V_value2.V_value2_len = N_value2;
    req.V_value2.V_value2_val = malloc(N_value2 * sizeof(double));
    memcpy(req.V_value2.V_value2_val, V_value2,
           N_value2 * sizeof(double));

    RPC_Coord rpc_c = { .x = coord.x, .y = coord.y };
    req.value3 = rpc_c;

    int *rp = modify_value_1(req, clnt);
    free(req.V_value2.V_value2_val);
    if (!rp) {
        clnt_perror(clnt, "Error RPC modify_value");
        clnt_destroy(clnt);
        return -1;
    }
    int status = *rp;
    clnt_destroy(clnt);
    return status;
}
