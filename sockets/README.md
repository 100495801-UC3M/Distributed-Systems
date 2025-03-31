# Distribuidos
100495833 - Javier Diez  
100495801 - Mario Hidalgo  

## Uso:

### Para compilar todo:
make

### Para ejecutar el servidor (en una terminal):
make servidor
./servidor <puerto>

### Para ejecutar el cliente (en otra terminal):
make cliente
env IP_TUPLAS=<ip> PORT_TUPLAS=<puerto> ./cliente

### Para limpiar los archivos:
make clear
