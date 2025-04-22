#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <glob.h>
#include <unistd.h>
#include "claves.h"

#define MAX_VALUE1_LEN 256
#define MAX_V2 32
#define FILENAME_FORMAT "record_%d.bin"
#define FILENAME_MAX_SIZE 64

// Estructura que se almacena en cada fichero.
typedef struct {
    int key;
    char value1[MAX_VALUE1_LEN];
    int N_value2;
    double V_value2[MAX_V2];
    struct Coord value3;
} Record;

// Mutex global para sincronizar el acceso a los ficheros.
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief Genera el nombre del fichero a partir de la clave.
 * @param key La clave del registro.
 * @param filename Buffer donde se almacena el nombre generado.
 * @param size Tamaño del buffer.
 */
static void get_filename(int key, char *filename, size_t size) {
    snprintf(filename, size, FILENAME_FORMAT, key);
}

/**
 * @brief Inicializa el servicio borrando todos los ficheros que contengan registros.
 * Se emplea glob para obtener todos los ficheros que coincidan con "record_*.bin".
 * @return 0 en caso de éxito y -1 en caso de error.
 */
int destroy(void) {
    pthread_mutex_lock(&mutex);
    glob_t glob_result;
    int ret = 0;
    if (glob("record_*.bin", 0, NULL, &glob_result) == 0) {
        for (size_t i = 0; i < glob_result.gl_pathc; i++) {
            if (remove(glob_result.gl_pathv[i]) != 0) {
                ret = -1;
            }
        }
    }
    globfree(&glob_result);
    pthread_mutex_unlock(&mutex);
    return ret;
}

/**
 * @brief Inserta un objeto <key, value1, value2, value3> creando un fichero nuevo.
 * Retorna -1 si la clave ya existe, si los parámetros son incorrectos o si ocurre un error.
 */
int set_value(int key, char *value1, int N_value2, double *V_value2, struct Coord value3) {
    if (strlen(value1) >= MAX_VALUE1_LEN)
        return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2)
        return -1;
    
    pthread_mutex_lock(&mutex);
    char filename[FILENAME_MAX_SIZE];
    get_filename(key, filename, sizeof(filename));

    // Verificar que no exista ya un fichero para esta clave.
    FILE *fp = fopen(filename, "rb");
    if (fp != NULL) {
        fclose(fp);
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    fp = fopen(filename, "wb");
    if (!fp) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }

    Record rec;
    rec.key = key;
    strncpy(rec.value1, value1, MAX_VALUE1_LEN - 1);
    rec.value1[MAX_VALUE1_LEN - 1] = '\0';
    rec.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) {
        rec.V_value2[i] = V_value2[i];
    }
    rec.value3 = value3;

    size_t written = fwrite(&rec, sizeof(Record), 1, fp);
    fclose(fp);
    pthread_mutex_unlock(&mutex);
    return (written == 1) ? 0 : -1;
}

/**
 * @brief Recupera los valores asociados a la clave key.
 * Abre el fichero correspondiente, lee el registro y copia los datos en los parámetros.
 * @return 0 en caso de éxito y -1 en caso de error.
 */
int get_value(int key, char *value1, int *N_value2, double *V_value2, struct Coord *value3) {
    pthread_mutex_lock(&mutex);
    char filename[FILENAME_MAX_SIZE];
    get_filename(key, filename, sizeof(filename));

    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    Record rec;
    if (fread(&rec, sizeof(Record), 1, fp) != 1) {
        fclose(fp);
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    fclose(fp);
    strncpy(value1, rec.value1, MAX_VALUE1_LEN);
    *N_value2 = rec.N_value2;
    for (int i = 0; i < rec.N_value2; i++) {
        V_value2[i] = rec.V_value2[i];
    }
    *value3 = rec.value3;
    pthread_mutex_unlock(&mutex);
    return 0;
}

/**
 * @brief Modifica los valores asociados a la clave key.
 * Sobrescribe el fichero correspondiente con los nuevos datos.
 * @return 0 en caso de éxito y -1 en caso de error.
 */
int modify_value(int key, char *value1, int N_value2, double *V_value2, struct Coord value3) {
    if (strlen(value1) >= MAX_VALUE1_LEN)
        return -1;
    if (N_value2 < 1 || N_value2 > MAX_V2)
        return -1;
    
    pthread_mutex_lock(&mutex);
    char filename[FILENAME_MAX_SIZE];
    get_filename(key, filename, sizeof(filename));

    FILE *fp = fopen(filename, "rb+");
    if (!fp) {
        pthread_mutex_unlock(&mutex);
        return -1;
    }
    Record rec;
    rec.key = key;
    strncpy(rec.value1, value1, MAX_VALUE1_LEN - 1);
    rec.value1[MAX_VALUE1_LEN - 1] = '\0';
    rec.N_value2 = N_value2;
    for (int i = 0; i < N_value2; i++) {
        rec.V_value2[i] = V_value2[i];
    }
    rec.value3 = value3;
    
    // Posicionarse al inicio y sobreescribir.
    fseek(fp, 0, SEEK_SET);
    size_t written = fwrite(&rec, sizeof(Record), 1, fp);
    fclose(fp);
    pthread_mutex_unlock(&mutex);
    return (written == 1) ? 0 : -1;
}

/**
 * @brief Borra el objeto asociado a la clave key eliminando el fichero correspondiente.
 * @return 0 en caso de éxito y -1 en caso de error.
 */
int delete_key(int key) {
    pthread_mutex_lock(&mutex);
    char filename[FILENAME_MAX_SIZE];
    get_filename(key, filename, sizeof(filename));
    int ret = remove(filename);
    pthread_mutex_unlock(&mutex);
    return (ret == 0) ? 0 : -1;
}

/**
 * @brief Determina si existe un objeto con la clave key comprobando la existencia del fichero.
 * @return 1 si existe, 0 si no existe y -1 en caso de error.
 */
int exist(int key) {
    pthread_mutex_lock(&mutex);
    char filename[FILENAME_MAX_SIZE];
    get_filename(key, filename, sizeof(filename));
    FILE *fp = fopen(filename, "rb");
    int ret = 0;
    if (fp) {
        ret = 1;
        fclose(fp);
    }
    pthread_mutex_unlock(&mutex);
    return ret;
}
