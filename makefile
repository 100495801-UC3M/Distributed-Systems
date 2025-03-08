# Compilador y opciones
CC = gcc
CFLAGS = -Wall -fPIC
LDFLAGS = -shared
LIBS = -lrt -lpthread

# Archivos fuente y objetos
PROXY_SRC = proxy-mq.c
PROXY_OBJ = proxy-mq.o
LIBRARY = libclaves.so

CLIENT_SRC = app-cliente.c
CLIENT_BIN = app-cliente

SERVER_SRC = servidor-mq.c
SERVER_BIN = servidor-mq

# Compilar la biblioteca compartida libclaves.so
$(LIBRARY): $(PROXY_OBJ)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

# Compilar proxy-mq.c como objeto
$(PROXY_OBJ): $(PROXY_SRC)
	$(CC) $(CFLAGS) -c $< -o $@

# Compilar el cliente app-cliente, enlazado con libclaves.so
$(CLIENT_BIN): $(CLIENT_SRC) $(LIBRARY)
	$(CC) -o $@ $< -L. -lclaves $(LIBS)

# Compilar el servidor servidor-mq
$(SERVER_BIN): $(SERVER_SRC)
	$(CC) -o $@ $< $(LIBS)

# Ejecutar el servidor
run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

# Ejecutar el cliente (usando la biblioteca dinámica)
run-client: $(CLIENT_BIN)
	LD_LIBRARY_PATH=. ./$(CLIENT_BIN)

# Limpiar archivos generados
clean:
	rm -f $(PROXY_OBJ) $(LIBRARY) $(CLIENT_BIN) $(SERVER_BIN)
