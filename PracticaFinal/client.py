from enum import Enum
import argparse
import socket
import sys
import threading

class client:
    # ******************** ENUMERACIÓN DE CÓDIGOS DE RETORNO ********************
    class RC(Enum):
        OK = 0
        USER_ERROR = 1
        ERROR = 2

    # ********************* ATRIBUTOS ESTÁTICOS *************************
    _server: str      = None       # Dirección IP o dominio del servidor
    _port:   int      = -1         # Puerto TCP del servidor
    _listen_sock      = None       # Socket de escucha P2P
    _stop:   bool     = False      # Flag para detener el servidor P2P
    _peers:  dict     = {}         # Cache de peers: nombre -> (ip, port)

    # ******************** PARSEO DE ARGUMENTOS *************************
    @staticmethod
    def parseArguments(argv) -> bool:
        """Parsea -s <server> y -p <port>."""
        parser = argparse.ArgumentParser(description="Cliente P2P")
        parser.add_argument('-s', type=str, required=True,
                            help='Server IP o nombre de dominio')
        parser.add_argument('-p', type=int, required=True,
                            help='Server TCP port')
        args = parser.parse_args(argv)
        client._server = args.s
        client._port   = args.p
        return True

    @staticmethod
    def usage():
        """Muestra ayuda de uso."""
        print("Usage: python3 client.py -s <server> -p <port>")

    # *********************** MÉTODOS DE OPERACIONES ***********************

    @staticmethod
    def register(user: str) -> "client.RC":
        """REGISTER <user>"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            s.sendall(b"REGISTER\0")
            s.sendall((user + "\0").encode())
            resp = s.recv(1)
            if not resp: raise RuntimeError
            code = resp[0]
            if code == client.RC.OK.value:
                print("REGISTER OK"); return client.RC.OK
            elif code == client.RC.USER_ERROR.value:
                print("USERNAME IN USE"); return client.RC.USER_ERROR
            else:
                print("REGISTER FAIL"); return client.RC.ERROR
        except:
            print("REGISTER FAIL"); return client.RC.ERROR
        finally:
            s.close()

    @staticmethod
    def unregister(user: str) -> "client.RC":
        """UNREGISTER <user>"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            s.sendall(b"UNREGISTER\0")
            s.sendall((user + "\0").encode())
            resp = s.recv(1)
            if not resp: raise RuntimeError
            code = resp[0]
            if code == client.RC.OK.value:
                print("UNREGISTER OK"); return client.RC.OK
            elif code == client.RC.USER_ERROR.value:
                print("USER DOES NOT EXIST"); return client.RC.USER_ERROR
            else:
                print("UNREGISTER FAIL"); return client.RC.ERROR
        except:
            print("UNREGISTER FAIL"); return client.RC.ERROR
        finally:
            s.close()

    # --------------- Servidor P2P para GET_FILE ---------------

    @staticmethod
    def _serve_peer(listen_sock):
        """Atiende peticiones GET_FILE entrantes."""
        listen_sock.listen(1)
        while not client._stop:
            try:
                peer, _ = listen_sock.accept()
            except OSError:
                break
            threading.Thread(target=client._handle_peer,
                             args=(peer,), daemon=True).start()
        listen_sock.close()

    @staticmethod
    def _handle_peer(peer):
        """Maneja una solicitud GET_FILE de un peer."""
        try:
            data = b''
            # Leer hasta 3 marcadores '\0': CMD, usuario, fichero
            while data.count(b'\0') < 3:
                chunk = peer.recv(1024)
                if not chunk: break
                data += chunk
            parts = data.split(b'\0')
            if parts[0].decode() == "GET_FILE" and len(parts) >= 3:
                filename = parts[2].decode()
                try:
                    with open(filename, 'rb') as f:
                        while True:
                            buf = f.read(1024)
                            if not buf: break
                            peer.sendall(buf)
                except FileNotFoundError:
                    pass
        except:
            pass
        peer.close()

    @staticmethod
    def _stop_peer():
        """Detiene el servidor P2P local."""
        client._stop = True
        try:
            client._listen_sock.close()
        except:
            pass

    # ****************** CONNECT / DISCONNECT ******************

    @staticmethod
    def connect(user: str) -> "client.RC":
        """CONNECT <user> <listen_port>"""
        # 1) Crear socket local en puerto dinámico
        listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_sock.bind(('', 0))
        port = listen_sock.getsockname()[1]
        client._listen_sock = listen_sock
        client._stop = False
        threading.Thread(target=client._serve_peer,
                         args=(listen_sock,), daemon=True).start()

        # 2) Enviar CONNECT al servidor central
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            s.sendall(b"CONNECT\0")
            s.sendall((user + "\0").encode())
            s.sendall((str(port) + "\0").encode())
            resp = s.recv(1)
            if not resp: raise RuntimeError
            code = resp[0]
            if code == client.RC.OK.value:
                print("CONNECT OK"); return client.RC.OK
            elif code == 1:
                print("CONNECT FAIL , USER DOES NOT EXIST"); return client.RC.USER_ERROR
            elif code == 2:
                print("USER ALREADY CONNECTED"); return client.RC.USER_ERROR
            else:
                print("CONNECT FAIL"); return client.RC.ERROR
        except:
            print("CONNECT FAIL"); return client.RC.ERROR
        finally:
            s.close()

    @staticmethod
    def disconnect(user: str) -> "client.RC":
        """DISCONNECT <user>"""
        client._stop_peer()
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            s.sendall(b"DISCONNECT\0")
            s.sendall((user + "\0").encode())
            resp = s.recv(1)
            if not resp: raise RuntimeError
            code = resp[0]
            if code == client.RC.OK.value:
                print("DISCONNECT OK"); return client.RC.OK
            elif code == 1:
                print("DISCONNECT FAIL , USER DOES NOT EXIST"); return client.RC.USER_ERROR
            elif code == 2:
                print("DISCONNECT FAIL , USER NOT CONNECTED"); return client.RC.USER_ERROR
            else:
                print("DISCONNECT FAIL"); return client.RC.ERROR
        except:
            print("DISCONNECT FAIL"); return client.RC.ERROR
        finally:
            s.close()

    # ****************** PUBLISH / DELETE ******************

    @staticmethod
    def publish(user: str, path: str, desc: str) -> "client.RC":
        """PUBLISH <user> <path> <desc>"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            s.sendall(b"PUBLISH\0")
            s.sendall((user + "\0").encode())
            s.sendall((path + "\0").encode())
            s.sendall((desc + "\0").encode())
            resp = s.recv(1)
            if not resp: raise RuntimeError
            code = resp[0]
            if code == client.RC.OK.value:
                print("PUBLISH OK"); return client.RC.OK
            elif code == 1:
                print("USER DOES NOT EXIST"); return client.RC.USER_ERROR
            elif code == 2:
                print("USER NOT CONNECTED"); return client.RC.USER_ERROR
            elif code == 3:
                print("CONTENT ALREADY PUBLISHED"); return client.RC.USER_ERROR
            else:
                print("PUBLISH FAIL"); return client.RC.ERROR
        except:
            print("PUBLISH FAIL"); return client.RC.ERROR
        finally:
            s.close()

    @staticmethod
    def delete(user: str, path: str) -> "client.RC":
        """DELETE <user> <path>"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            s.sendall(b"DELETE\0")
            s.sendall((user + "\0").encode())
            s.sendall((path + "\0").encode())
            resp = s.recv(1)
            if not resp: raise RuntimeError
            code = resp[0]
            if code == client.RC.OK.value:
                print("DELETE OK"); return client.RC.OK
            elif code == 1:
                print("USER DOES NOT EXIST"); return client.RC.USER_ERROR
            elif code == 2:
                print("USER NOT CONNECTED"); return client.RC.USER_ERROR
            elif code == 3:
                print("CONTENT NOT PUBLISHED"); return client.RC.USER_ERROR
            else:
                print("DELETE FAIL"); return client.RC.ERROR
        except:
            print("DELETE FAIL"); return client.RC.ERROR
        finally:
            s.close()

    # ****************** LIST_USERS / LIST_CONTENT ******************

    @staticmethod
    def listusers(user: str) -> "client.RC":
        """LIST_USERS <user>"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            s.sendall(b"LIST_USERS\0")
            s.sendall((user + "\0").encode())
            resp = s.recv(1)
            if not resp: raise RuntimeError
            code = resp[0]
            if code != client.RC.OK.value:
                if code == client.RC.USER_ERROR.value:
                    print("USER DOES NOT EXIST")
                elif code == 2:
                    print("USER NOT CONNECTED")
                else:
                    print("LIST_USERS FAIL")
                return client.RC.USER_ERROR
            # OK: recibir número de usuarios
            data = b''
            while not data.endswith(b'\0'):
                chunk = s.recv(1)
                if not chunk: raise RuntimeError
                data += chunk
            count = int(data[:-1])
            client._peers.clear()
            for _ in range(count):
                # recibir nombre
                nb = b''
                while True:
                    b = s.recv(1)
                    if not b or b == b'\0': break
                    nb += b
                name = nb.decode()
                # IP
                ib = b''
                while True:
                    b = s.recv(1)
                    if not b or b == b'\0': break
                    ib += b
                ip = ib.decode()
                # puerto
                pb = b''
                while True:
                    b = s.recv(1)
                    if not b or b == b'\0': break
                    pb += b
                port = int(pb.decode())
                client._peers[name] = (ip, port)
            # imprimir
            print("LIST_USERS:")
            for name, (ip, port) in client._peers.items():
                print(f"{name}\t{ip}:{port}")
            return client.RC.OK
        except:
            print("LIST_USERS FAIL"); return client.RC.ERROR
        finally:
            s.close()

    @staticmethod
    def listcontent(user: str, target: str) -> "client.RC":
        """LIST_CONTENT <user> <target>"""
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.connect((client._server, client._port))
            s.sendall(b"LIST_CONTENT\0")
            s.sendall((user + "\0").encode())
            s.sendall((target + "\0").encode())
            resp = s.recv(1)
            if not resp: raise RuntimeError
            code = resp[0]
            if code != client.RC.OK.value:
                if code == 1:
                    print("USER DOES NOT EXIST")
                elif code == 2:
                    print("USER NOT CONNECTED")
                elif code == 3:
                    print("REMOTE USER DOES NOT EXIST")
                else:
                    print("LIST_CONTENT FAIL")
                return client.RC.USER_ERROR
            data = b''
            while not data.endswith(b'\0'):
                chunk = s.recv(1)
                if not chunk: raise RuntimeError
                data += chunk
            count = int(data[:-1])
            files = []
            for _ in range(count):
                fb = b''
                while True:
                    b = s.recv(1)
                    if not b or b == b'\0': break
                    fb += b
                files.append(fb.decode())
            print("FILES:")
            for fn in files:
                print(fn)
            return client.RC.OK
        except:
            print("LIST_CONTENT FAIL"); return client.RC.ERROR
        finally:
            s.close()

    # ****************** GET_FILE ******************

    @staticmethod
    def getfile(user: str, target: str, filename: str) -> "client.RC":
        """GET_FILE <user> <target> <filename>"""
        # Asegurar que tenemos info del peer
        if target not in client._peers:
            res = client.listusers(user)
            if res != client.RC.OK:
                return res
            if target not in client._peers:
                print("REMOTE USER DOES NOT EXIST")
                return client.RC.USER_ERROR
        ip, port = client._peers[target]
        # Conectar al peer
        try:
            p = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            p.connect((ip, port))
            p.sendall(b"GET_FILE\0")
            p.sendall((target + "\0").encode())
            p.sendall((filename + "\0").encode())
            with open(filename, 'wb') as f:
                while True:
                    chunk = p.recv(1024)
                    if not chunk: break
                    f.write(chunk)
            print("GET_FILE OK"); return client.RC.OK
        except:
            print("GET_FILE FAIL"); return client.RC.ERROR
        finally:
            p.close()

    # ******************** INTÉRPRETE DE COMANDOS ********************

    @staticmethod
    def shell():
        """
        Intérprete de comandos:
          REGISTER <user>
          UNREGISTER <user>
          CONNECT <user>
          DISCONNECT <user>
          PUBLISH <user> <path> <desc>
          DELETE <user> <path>
          LIST_USERS <user>
          LIST_CONTENT <user> <target>
          GET_FILE <user> <target> <filename>
          EXIT / QUIT para salir.
        """
        while True:
            try:
                line = input("c> ")
            except EOFError:
                print()
                break
            if not line.strip():
                continue
            parts = line.strip().split()
            cmd = parts[0].upper()
            args = parts[1:]
            if cmd in ("EXIT", "QUIT"):
                break
            elif cmd == "REGISTER"   and len(args) == 1:
                client.register(args[0])
            elif cmd == "UNREGISTER" and len(args) == 1:
                client.unregister(args[0])
            elif cmd == "CONNECT"    and len(args) == 1:
                client.connect(args[0])
            elif cmd == "DISCONNECT" and len(args) == 1:
                client.disconnect(args[0])
            elif cmd == "PUBLISH"    and len(args) >= 3:
                user, path = args[0], args[1]
                desc = " ".join(args[2:])
                client.publish(user, path, desc)
            elif cmd == "DELETE"     and len(args) == 2:
                client.delete(args[0], args[1])
            elif cmd == "LIST_USERS" and len(args) == 1:
                client.listusers(args[0])
            elif cmd == "LIST_CONTENT" and len(args) == 2:
                client.listcontent(args[0], args[1])
            elif cmd == "GET_FILE" and len(args) == 3:
                client.getfile(args[0], args[1], args[2])
            else:
                print("Sintaxis inválida o comando desconocido")
        print("Cliente terminado.")

    @staticmethod
    def main(argv):
        if not client.parseArguments(argv):
            client.usage()
            return
        client.shell()

if __name__ == "__main__":
    client.main(sys.argv[1:])