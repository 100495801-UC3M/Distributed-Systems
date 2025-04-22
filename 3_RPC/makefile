# Variables del compilador y flags
CC = gcc
CFLAGS = -Wall -O2 -g -fPIC
RPCGEN = rpcgen
CFLAGS += -g -I/usr/include/tirpc
LDLIBS += -lnsl -lpthread -ldl -ltirpc
# Archivo de la interfaz RPC
RPC_X = tuplas.x

# Regla para generar los archivos RPC (tuplas.h, tuplas_clnt.c, tuplas_svc.c, tuplas_xdr.c)
tuplas.h tuplas_clnt.c tuplas_svc.c tuplas_xdr.c: $(RPC_X)
	rm -f tuplas.h tuplas_clnt.c tuplas_svc.c tuplas_xdr.c tuplas_client.c tuplas_server.c
	$(RPCGEN) -a $(RPC_X)

# Objetos para el cliente y el servidor
OBJS_CLIENT = claves.o proxy-rpc.o appcliente.o tuplas_clnt.o tuplas_xdr.o
OBJS_SERVER = claves.o servidor-rpc.o tuplas_svc.o tuplas_xdr.o

# Biblioteca compartida
LIB = libclaves.so

# Ejecutables
CLIENT_EXE = appcliente
SERVER_EXE = servidor

.PHONY: all clean rpcgen_files

all: tuplas.h tuplas_clnt.c tuplas_svc.c tuplas_xdr.c $(LIB) $(CLIENT_EXE) $(SERVER_EXE)

# Compilación de la biblioteca libclaves.so
$(LIB): claves.o proxy-rpc.o
	$(CC) -shared -o $(LIB) claves.o proxy-rpc.o

# Compilar claves.c
claves.o: claves.c claves.h
	$(CC) $(CFLAGS) -c claves.c -o claves.o

# Compilar proxy-rpc.c
proxy-rpc.o: proxy-rpc.c tuplas.h
	$(CC) $(CFLAGS) -c proxy-rpc.c -o proxy-rpc.o

# Compilar appcliente.c
appcliente.o: appcliente.c claves.h tuplas.h
	$(CC) $(CFLAGS) -c appcliente.c -o appcliente.o

# Compilar servidor-rpc.c
servidor-rpc.o: servidor-rpc.c tuplas.h claves.h
	$(CC) $(CFLAGS) -c servidor-rpc.c -o servidor-rpc.o

# Compilar los archivos generados por rpcgen
tuplas_clnt.o: tuplas_clnt.c tuplas.h
	$(CC) $(CFLAGS) -c tuplas_clnt.c -o tuplas_clnt.o

tuplas_svc.o: tuplas_svc.c tuplas.h
	$(CC) $(CFLAGS) -c tuplas_svc.c -o tuplas_svc.o

tuplas_xdr.o: tuplas_xdr.c tuplas.h
	$(CC) $(CFLAGS) -c tuplas_xdr.c -o tuplas_xdr.o

# Enlazar el ejecutable del cliente
$(CLIENT_EXE): $(OBJS_CLIENT)
	$(CC) $(CFLAGS) -o $(CLIENT_EXE) $(OBJS_CLIENT) -L. -lclaves -lnsl -lresolv

# Enlazar el ejecutable del servidor
$(SERVER_EXE): $(OBJS_SERVER)
	$(CC) $(CFLAGS) -o $(SERVER_EXE) $(OBJS_SERVER) -lnsl -lresolv

clean:
	rm -f *.o $(CLIENT_EXE) $(SERVER_EXE) $(LIB) tuplas_clnt.c tuplas_svc.c tuplas_xdr.c tuplas.h tuplas_client.c tuplas_server.c Makefile.tuplas *~



