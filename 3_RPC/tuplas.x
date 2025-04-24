/* tuplas.x*/
struct RPC_Coord {
    int x;
    int y;
};

struct request {
    int          key;
    string       value1<256>;
    int          N_value2;
    double       V_value2<256>;
    RPC_Coord    value3;
};

struct response {
    int          status;        /* 0=éxito, !=0 error */
    string       value1<256>;
    int          N_value2;
    double       V_value2<256>;
    RPC_Coord    value3;
};

program TUPLAS_PROG {
    version TUPLAS_VERS {
        int       DESTROY(request)    = 1;
        int       SET_VALUE(request)  = 2;
        response  GET_VALUE(request)  = 3;
        int       DELETE_KEY(request) = 4;
        int       MODIFY_VALUE(request)= 5;
        int       EXIST(request)      = 6;
    } = 1;
} = 0x20000001;
