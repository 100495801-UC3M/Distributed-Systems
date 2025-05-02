#!/usr/bin/env python3
import socket
import argparse
import threading
import sys
from enum import Enum

class client:
    class RC(Enum):
        OK = 0
        USER_ERROR = 1
        ERROR = 2

    # Parámetros del servidor central
    _server: str    = None
    _port:   int    = -1

    # Sesión con el servidor central
    _sock         = None   # Socket TCP con el servidor central
    _username: str = None

    # P2P local
    _listen_sock = None    # Socket TCP de escucha P2P
    _stop        = False   # Flag para detener el mini‐servidor P2P

    # Cache de peers (actualizada en LIST_USERS)
    _peers = {}  # name -> (ip, port)

    # ---------------- Argumentos ----------------
    @staticmethod
    def parseArguments(argv) -> bool:
        parser = argparse.ArgumentParser(description="Cliente P2P Parte 1")
        parser.add_argument('-s', required=True,
                            help='IP o dominio del servidor central')
        parser.add_argument('-p', type=int, default=8080,
                            help='Puerto TCP del servidor central (por defecto: 8080)')
        args = parser.parse_args(argv)
        client._server = args.s
        client._port   = args.p
        return True

    @staticmethod
    def usage():
        print("Usage: python3 client.py -s <server> [-p <port>]")

    # ---------------- P2P Entrante ----------------
    @staticmethod
    def _serve_peer(listen_sock):
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
        try:
            data = b''
            # Esperamos 3 marcadores \0: CMD, username, filename
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
                    # Silent fail
                    pass
        except:
            pass
        peer.close()

    @staticmethod
    def _stop_peer():
        client._stop = True
        try:
            client._listen_sock.close()
        except:
            pass

    # ---------------- REGISTER / UNREGISTER ----------------
    @staticmethod
    def register(user: str) -> "client.RC":
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((client._server, client._port))
                s.sendall(b"REGISTER\0" + user.encode() + b'\0')
                code = s.recv(1)[0]
            if code == client.RC.OK.value:
                print("REGISTER OK"); return client.RC.OK
            elif code == client.RC.USER_ERROR.value:
                print("USERNAME IN USE"); return client.RC.USER_ERROR
            else:
                print("REGISTER FAIL"); return client.RC.ERROR
        except:
            print("REGISTER FAIL"); return client.RC.ERROR

    @staticmethod
    def unregister(user: str) -> "client.RC":
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((client._server, client._port))
                s.sendall(b"UNREGISTER\0" + user.encode() + b'\0')
                code = s.recv(1)[0]
            if code == client.RC.OK.value:
                print("UNREGISTER OK"); return client.RC.OK
            elif code == client.RC.USER_ERROR.value:
                print("USER DOES NOT EXIST"); return client.RC.USER_ERROR
            else:
                print("UNREGISTER FAIL"); return client.RC.ERROR
        except:
            print("UNREGISTER FAIL"); return client.RC.ERROR

    # ---------------- CONNECT / DISCONNECT ----------------
    @staticmethod
    def connect(user: str) -> "client.RC":
        if client._sock is not None:
            print("ALREADY CONNECTED"); return client.RC.ERROR

        # 1) Preparamos mini‐servidor P2P
        listen_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listen_sock.bind(('', 0))
        port_local = listen_sock.getsockname()[1]
        client._listen_sock = listen_sock
        client._stop = False
        threading.Thread(target=client._serve_peer,
                         args=(listen_sock,), daemon=True).start()

        # 2) Conectamos al servidor central y enviamos puerto P2P
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.connect((client._server, client._port))
            # protocolo: "CONNECT\0<user>\0<port>\0"
            sock.sendall(b"CONNECT\0" +
                         user.encode() + b'\0' +
                         str(port_local).encode() + b'\0')
            code = sock.recv(1)[0]
            if code == client.RC.OK.value:
                client._sock = sock
                client._username = user
                print("CONNECT OK")
                return client.RC.OK
            elif code == 1:
                print("CONNECT FAIL , USER DOES NOT EXIST")
            elif code == 2:
                print("USER ALREADY CONNECTED")
            else:
                print("CONNECT FAIL")
        except:
            print("CONNECT FAIL")
        # En error, cerramos y paramos P2P
        sock.close()
        client._stop_peer()
        return client.RC.USER_ERROR if code in (1,2) else client.RC.ERROR

    @staticmethod
    def disconnect() -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR
        s = client._sock
        try:
            s.sendall(b"DISCONNECT\0")
            code = s.recv(1)[0]
            s.close()
            client._sock = None
            client._username = None
            client._stop_peer()
            if code == client.RC.OK.value:
                print("DISCONNECT OK"); return client.RC.OK
        except:
            pass
        print("DISCONNECT FAIL")
        return client.RC.ERROR

    # ---------------- PUBLISH ----------------
    @staticmethod
    def publish(file_name: str, desc: str) -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR
        s = client._sock
        # "PUBLISH\0<file>\0<desc>\0"
        s.sendall(b"PUBLISH\0" + file_name.encode() + b'\0' +
                  desc.encode() + b'\0')
        code = s.recv(1)[0]
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

    # ---------------- DELETE ----------------
    @staticmethod
    def delete(file_name: str) -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR
        s = client._sock
        s.sendall(b"DELETE\0" + file_name.encode() + b'\0')
        code = s.recv(1)[0]
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

    # ---------------- LIST_USERS ----------------
    @staticmethod
    def list_users() -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR
        s = client._sock
        s.sendall(b"LIST_USERS\0")
        code = s.recv(1)[0]
        if code != client.RC.OK.value:
            if code == client.RC.USER_ERROR.value:
                print("USER DOES NOT EXIST")
            elif code == 2:
                print("USER NOT CONNECTED")
            else:
                print("LIST_USERS FAIL")
            return client.RC.USER_ERROR

        # OK: recibimos count\0
        data = b''
        while not data.endswith(b'\0'):
            data += s.recv(1)
        count = int(data[:-1])
        client._peers.clear()
        print("LIST_USERS:")
        for _ in range(count):
            # nombre
            nb = b''
            while True:
                c = s.recv(1)
                if c == b'\0': break
                nb += c
            name = nb.decode()
            # IP
            ib = b''
            while True:
                c = s.recv(1)
                if c == b'\0': break
                ib += c
            ip = ib.decode()
            # puerto
            pb = b''
            while True:
                c = s.recv(1)
                if c == b'\0': break
                pb += c
            port = int(pb.decode())
            client._peers[name] = (ip, port)
            print(f"{name}\t{ip}:{port}")

        return client.RC.OK

    # ---------------- LIST_CONTENT ----------------
    @staticmethod
    def list_content(target: str) -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR
        s = client._sock
        s.sendall(b"LIST_CONTENT\0" + target.encode() + b'\0')
        code = s.recv(1)[0]
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
            data += s.recv(1)
        count = int(data[:-1])
        print("FILES:")
        for _ in range(count):
            fb = b''
            while True:
                c = s.recv(1)
                if c == b'\0': break
                fb += c
            print(fb.decode())
        return client.RC.OK

    # ---------------- GET_FILE ----------------
    @staticmethod
    def get_file(remote_user: str, remote_file: str, local_file: str) -> "client.RC":
        # Asegurarnos de tener la IP y puerto del remote_user
        if remote_user not in client._peers:
            res = client.list_users()
            if res != client.RC.OK or remote_user not in client._peers:
                print("REMOTE USER DOES NOT EXIST")
                return client.RC.USER_ERROR

        ip, port = client._peers[remote_user]
        try:
            p = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            p.connect((ip, port))
            # Protocolo P2P GET_FILE
            p.sendall(b"GET_FILE\0" +
                      client._username.encode() + b'\0' +
                      remote_file.encode() + b'\0')
            with open(local_file, 'wb') as f:
                while True:
                    chunk = p.recv(1024)
                    if not chunk: break
                    f.write(chunk)
            print("GET_FILE OK")
            return client.RC.OK
        except:
            print("GET_FILE FAIL")
            return client.RC.ERROR
        finally:
            p.close()

    # ---------------- Shell ----------------
    @staticmethod
    def shell():
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
                if client._sock: client.disconnect()
                break
            elif cmd == "REGISTER"   and len(args) == 1:
                client.register(args[0])
            elif cmd == "UNREGISTER" and len(args) == 1:
                client.unregister(args[0])
            elif cmd == "CONNECT"    and len(args) == 1:
                client.connect(args[0])
            elif cmd == "PUBLISH"    and len(args) >= 2:
                fn, desc = args[0], " ".join(args[1:])
                client.publish(fn, desc)
            elif cmd == "DELETE"     and len(args) == 1:
                client.delete(args[0])
            elif cmd == "LIST_USERS" and len(args) == 0:
                client.list_users()
            elif cmd == "LIST_CONTENT" and len(args) == 1:
                client.list_content(args[0])
            elif cmd == "GET_FILE"   and len(args) == 3:
                client.get_file(args[0], args[1], args[2])
            elif cmd == "DISCONNECT" and len(args) == 0:
                client.disconnect()
            else:
                print("Sintaxis inválida")

        print("Cliente terminado.")

    @staticmethod
    def main(argv):
        if not client.parseArguments(argv):
            client.usage()
            return
        client.shell()

if __name__ == "__main__":
    client.main(sys.argv[1:])
