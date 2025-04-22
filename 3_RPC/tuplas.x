/* tuplas.x: Archivo de definición de la interfaz RPC para el servicio de claves */
%%

/* Definición de las estructuras utilizadas en las llamadas RPC */
struct request {
    string key<256>;
    string value<256>;
};

struct response {
    int status;       /* 0: éxito, diferente de 0: error */
    string value<256>;/* Utilizado en get_value */
};

program TUPLAS_PROG {
    version TUPLAS_VERS {
        int DESTROY(request) = 1;
        int SET_VALUE(request) = 2;
        response GET_VALUE(request) = 3;
        int DELETE_KEY(request) = 4;
        int MODIFY_VALUE(request) = 5;
        int EXIST(request) = 6;
    } = 1;
} = 0x20000001;
