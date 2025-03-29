# Compilador y opciones
CC = gcc
CFLAGS = -Wall -fPIC
LDFLAGS = -shared
LIBS = -lrt -lpthread

# Archivos fuente y objetos
PROXY_SRC = proxy-sock.c
PROXY_OBJ = proxy-sock.o
LIBRARY = libclaves.so

CLIENT_SRC = app-cliente.c
CLIENT_BIN = app-cliente

SERVER_SRC = servidor-sock.c
SERVER_EXTRA = claves.c
SERVER_BIN = servidor-sock

# Compilar la biblioteca compartida libclaves.so (usada por el cliente)
$(LIBRARY): $(PROXY_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Compilar proxy-sock.c a objeto
$(PROXY_OBJ): $(PROXY_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Compilar el cliente app-cliente, enlazado con libclaves.so
$(CLIENT_BIN): $(CLIENT_SRC) $(LIBRARY)
	$(CC) -o $@ $< -L. -lclaves $(LIBS)

# Compilar el servidor, incluyendo claves.c (que tiene la implementación real en el lado del servidor)
$(SERVER_BIN): $(SERVER_SRC) $(SERVER_EXTRA)
	$(CC) -o $@ $(SERVER_SRC) $(SERVER_EXTRA) $(LIBS)

# Ejecutar el servidor y el cliente con el mismo puerto el comando es: make run-server PORT=<puerto>
run-server: $(SERVER_BIN) $(CLIENT_BIN)
	@ if [ -z "$(PORT)" ]; then \
		echo "Uso: make run-server PORT=<puerto>"; \
		exit 1; \
	  fi; \
	echo "Iniciando servidor en el puerto $$PORT..."; \
	SERVER_PORT=$$PORT ./$(SERVER_BIN) $$PORT & \
	sleep 2; \
	echo "Iniciando cliente en el puerto $$PORT..."; \
	SERVER_PORT=$$PORT LD_LIBRARY_PATH=. ./$(CLIENT_BIN); \
	wait

# Limpiar archivos generados
clean:
	rm -f $(PROXY_OBJ) $(LIBRARY) $(CLIENT_BIN) $(SERVER_BIN)
