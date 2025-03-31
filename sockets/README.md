# Distribuidos
100495833 - Javier Diez  
100495801 - Mario Hidalgo  

## Uso:

### Para compilar todo:
make

### Para ejecutar el servidor (en una terminal):
make servidor
./servidor <puerto>
## Ejemplo:
./servidor 8080

### Para ejecutar el cliente (en otra terminal):
make cliente
export IP_TUPLAS=<ip> PORT_TUPLAS=<puerto>
env IP_TUPLAS=<ip>PORT_TUPLAS=<puerto> ./cliente
## Ejemplo:
export IP_TUPLAS=127.0.0.1 PORT_TUPLAS=8080
env IP_TUPLAS=127.0.0.1 PORT_TUPLAS=8080 ./cliente

### Para limpiar los archivos:
make clear
