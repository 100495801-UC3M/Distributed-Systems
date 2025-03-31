#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "claves.h"

#define MAX_STR_LEN 256
#define MAX_VECTOR 32

// Función auxiliar para imprimir el contenido de una tupla.
void print_tuple(int key, const char *value1, int N_value2, double *V_value2, struct Coord value3) {
    printf("Key: %d\n", key);
    printf("Value1: %s\n", value1);
    printf("N_value2: %d\n", N_value2);
    printf("V_value2: ");
    for (int i = 0; i < N_value2; i++) {
        printf("%.2f ", V_value2[i]);
    }
    printf("\nCoord: (%d, %d)\n", value3.x, value3.y);
    printf("---------------------------------\n");
}

int main(void) {
    // Leer las variables de entorno IP_TUPLAS y PORT_TUPLAS
    char *ip = getenv("IP_TUPLAS");
    char *port_str = getenv("PORT_TUPLAS");
    if (!ip || !port_str) {
        fprintf(stderr, "Error: Las variables de entorno IP_TUPLAS y PORT_TUPLAS deben estar definidas.\n");
        return 1;
    }
    int port = atoi(port_str);

    // Configurar el proxy con las variables de entorno
    init_proxy(ip, port);

    int err;
    char value1[MAX_STR_LEN];
    int N_value2;
    double V_value2[MAX_VECTOR];
    struct Coord value3;

    printf("===== TEST 1: destroy() =====\n");
    err = destroy();
    printf("destroy() retornó: %d\n", err);
    // Verificar que no exista una clave conocida (por ejemplo, 5)
    err = exist(5);
    printf("Después de destroy(), exist(5) retornó: %d (esperado 0)\n\n", err);

    // --- TESTS DE set_value() ---
    printf("===== TEST 2: set_value() - Inserciones válidas =====\n");
    // Preparar datos de prueba
    char *v1 = "Valor inicial";
    double v2[] = {2.3, 0.5, 23.45};
    struct Coord v3 = {10, 5};
    
    // Insertar varias claves válidas
    int keys[] = {1, 2, 3, 4, 5};
    for (int i = 0; i < 5; i++) {
        err = set_value(keys[i], v1, 3, v2, v3);
        printf("set_value() para clave %d retornó: %d (esperado 0)\n", keys[i], err);
    }
    printf("\n");

    printf("===== TEST 3: set_value() - Inserción duplicada =====\n");
    // Intentar insertar duplicado para la clave 3
    err = set_value(3, v1, 3, v2, v3);
    printf("set_value() duplicada para clave 3 retornó: %d (esperado -1)\n\n", err);

    printf("===== TEST 4: get_value() - Recuperaciones válidas =====\n");
    for (int i = 0; i < 5; i++) {
        err = get_value(keys[i], value1, &N_value2, V_value2, &value3);
        printf("get_value() para clave %d retornó: %d\n", keys[i], err);
        if (err == 0) {
            print_tuple(keys[i], value1, N_value2, V_value2, value3);
        } else {
            printf("Error al recuperar clave %d\n", keys[i]);
        }
    }
    // Intentar recuperar una clave inexistente
    err = get_value(100, value1, &N_value2, V_value2, &value3);
    printf("get_value() para clave inexistente 100 retornó: %d (esperado -1)\n\n", err);

    printf("===== TEST 5: modify_value() =====\n");
    // Modificar una clave existente (por ejemplo, clave 4)
    char *new_v1 = "Valor modificado";
    double new_v2[] = {9.9, 8.8};
    struct Coord new_v3 = {30, 40};
    err = modify_value(4, new_v1, 2, new_v2, new_v3);
    printf("modify_value() para clave 4 retornó: %d (esperado 0)\n", err);
    // Verificar modificación
    err = get_value(4, value1, &N_value2, V_value2, &value3);
    if (err == 0) {
        printf("Después de modificar, datos para clave 4:\n");
        print_tuple(4, value1, N_value2, V_value2, value3);
    } else {
        printf("Error al recuperar la clave 4 modificada\n");
    }
    // Intentar modificar una clave inexistente
    err = modify_value(200, new_v1, 2, new_v2, new_v3);
    printf("modify_value() para clave inexistente 200 retornó: %d (esperado -1)\n\n", err);

    printf("===== TEST 6: set_value() - Validación de parámetros =====\n");
    // Caso: value1 demasiado larga (más de 255 caracteres)
    char long_str[MAX_STR_LEN + 50];
    memset(long_str, 'A', sizeof(long_str) - 1);
    long_str[sizeof(long_str) - 1] = '\0';
    err = set_value(10, long_str, 3, v2, v3);
    printf("set_value() para clave 10 con cadena larga retornó: %d (esperado -1)\n", err);
    // Caso: N_value2 fuera de rango: 0
    err = set_value(11, v1, 0, v2, v3);
    printf("set_value() para clave 11 con N_value2=0 retornó: %d (esperado -1)\n", err);
    // Caso: N_value2 fuera de rango: 33
    err = set_value(12, v1, MAX_VECTOR+1, v2, v3);
    printf("set_value() para clave 12 con N_value2=33 retornó: %d (esperado -1)\n\n", err);

    printf("===== TEST 7: exist() =====\n");
    // Verificar existencia de claves ya insertadas y de claves inexistentes.
    for (int i = 0; i < 5; i++) {
        err = exist(keys[i]);
        printf("exist() para clave %d retornó: %d (esperado 1)\n", keys[i], err);
    }

    printf("===== TEST 8: delete_key() =====\n");
    // Borrar una clave existente (por ejemplo, clave 2)
    err = delete_key(2);
    printf("delete_key() para clave 2 retornó: %d (esperado 0)\n", err);
    // Verificar que ya no existe
    err = exist(2);
    printf("exist() para clave 2 tras borrado retornó: %d (esperado 0)\n", err);
    // Intentar borrar una clave inexistente
    err = delete_key(300);
    printf("delete_key() para clave inexistente 300 retornó: %d (esperado -1)\n\n", err);

    printf("===== TEST 9: Inserción de registros límite =====\n");
    // Cadena de 255 caracteres (límite válido)
    char boundary_str[256];
    memset(boundary_str, 'B', 255);
    boundary_str[255] = '\0';
    err = set_value(50, boundary_str, 3, v2, v3);
    printf("set_value() para clave 50 con cadena de 255 caracteres retornó: %d (esperado 0)\n", err);
    // Verificar con get_value
    err = get_value(50, value1, &N_value2, V_value2, &value3);
    if (err == 0) {
        printf("Datos para clave 50:\n");
        print_tuple(50, value1, N_value2, V_value2, value3);
    }
    // Intentar con cadena demasiado larga (>255)
    char too_long_str[270];
    memset(too_long_str, 'C', sizeof(too_long_str) - 1);
    too_long_str[sizeof(too_long_str)-1] = '\0';
    err = set_value(51, too_long_str, 3, v2, v3);
    printf("set_value() para clave 51 con cadena de 269 caracteres retornó: %d (esperado -1)\n\n", err);

    printf("===== TEST 10: Validación de N_value2 =====\n");
    // N_value2 mínimo válido = 1
    double single_val[1] = {42.0};
    err = set_value(60, "Un solo elemento", 1, single_val, v3);
    printf("set_value() para clave 60 con N_value2=1 retornó: %d (esperado 0)\n", err);
    // N_value2 máximo válido = 32
    double vec32[32];
    for (int i = 0; i < 32; i++) {
        vec32[i] = i * 1.0;
    }
    err = set_value(61, "32 elementos", 32, vec32, v3);
    printf("set_value() para clave 61 con N_value2=32 retornó: %d (esperado 0)\n", err);
    // N_value2 fuera de rango: 0 y 33 ya se han probado

    printf("\n===== TEST 11: destroy() final =====\n");
    // Insertar varias claves y luego llamar a destroy()
    int preDestroyKeys[] = {70, 71, 72};
    for (int i = 0; i < 3; i++) {
        err = set_value(preDestroyKeys[i], "PreDestroy", 3, v2, v3);
        printf("Insertado clave %d, resultado: %d\n", preDestroyKeys[i], err);
    }
    printf("Llamando a destroy()...\n");
    err = destroy();
    printf("destroy() retornó: %d\n", err);
    for (int i = 0; i < 3; i++) {
        err = exist(preDestroyKeys[i]);
        printf("Después de destroy(), exist(%d) retornó: %d (esperado 0)\n", preDestroyKeys[i], err);
    }
    
    printf("\n===== TODOS LOS TESTS FINALIZADOS =====\n");
    return 0;
}
