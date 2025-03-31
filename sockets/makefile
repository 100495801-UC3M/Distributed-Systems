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
CLIENT_BIN = cliente

SERVER_SRC = servidor-sock.c
SERVER_EXTRA = claves.c
SERVER_BIN = servidor

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

# Compilar el servidor, incluyendo claves.c (que tiene la implementación real en el lado del servidor)
servidor: $(SERVER_BIN)

# Ejecutar el cliente
run-client: $(CLIENT_BIN)
	@ if [ -z "$(IP_TUPLAS)" ] || [ -z "$(PORT_TUPLAS)" ]; then \
		echo "Uso: env IP_TUPLAS=<ip> PORT_TUPLAS=<puerto> make run-client"; \
		exit 1; \
	  fi; \
	echo "Iniciando cliente con IP=$$IP_TUPLAS y PORT=$$PORT_TUPLAS..."; \
	LD_LIBRARY_PATH=. ./$(CLIENT_BIN)

# Limpiar archivos generados
clean:
	rm -f $(PROXY_OBJ) $(LIBRARY) $(CLIENT_BIN) $(SERVER_BIN)
