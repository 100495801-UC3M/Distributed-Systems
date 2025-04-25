# Distribuidos
100495833 - Javier Diez  
100495801 - Mario Hidalgo  

## Uso:

### Para compilar todo:
make

### Para ejecutar el servidor (en una terminal):
make servidor
./servidor 

### Para ejecutar el cliente (en otra terminal):
make appcliente
export IP_TUPLAS=<ip> 
env IP_TUPLAS=<ip> ./appcliente
## Ejemplo:
export IP_TUPLAS=localhost
./appcliente
## NOTA
Si los tests no se ejecutan directamente (no se encuentra "libclaves.so"), ejecutar
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
./appcliente

### Para limpiar los archivos:
make clean
