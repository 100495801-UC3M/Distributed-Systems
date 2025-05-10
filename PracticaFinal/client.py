import socket
import argparse
import threading
import sys
import os
import urllib.request
import urllib.error
from enum import Enum

class client:
    class RC(Enum):
        OK = 0
        USER_ERROR = 1 # Para errores relacionados con el usuario/estado
        ERROR = 2      # Para errores generales/de comunicación
        # Códigos específicos para GET_FILE P2P
        P2P_FILE_NOT_FOUND = 1
        P2P_OTHER_ERROR = 2


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
        parser = argparse.ArgumentParser(description="Cliente P2P Parte 1 y 2") # Actualizado
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

    # ---------------- Timestamp Service ----------------
    @staticmethod
    def _get_current_timestamp() -> str:
        url_to_fetch = "http://localhost:5000/getTime" # Asegúrate que el puerto es correcto
        try:
            with urllib.request.urlopen(url_to_fetch, timeout=2) as response: # Aumenta un poco el timeout por si acaso
                if response.status == 200:
                    timestamp_data = response.read().decode('utf-8')
                    return timestamp_data
                else:
                    return "TIMESTAMP_ERROR" 
        except ConnectionRefusedError:
            return "TIMESTAMP_UNAVAILABLE"
        except socket.timeout:
            return "TIMESTAMP_UNAVAILABLE"
        except urllib.error.URLError as e:
            return "TIMESTAMP_UNAVAILABLE"
        except Exception as e:
            return "TIMESTAMP_ERROR"

    # ---------------- P2P Entrante ----------------
    @staticmethod
    def _serve_peer(listen_sock):
        listen_sock.listen(1)
        while not client._stop:
            try:
                peer, _ = listen_sock.accept()
                if client._stop: 
                    peer.close()
                    break
            except OSError:
                break 
            threading.Thread(target=client._handle_peer,
                             args=(peer,), daemon=True).start()
        if listen_sock:
            try:
                listen_sock.close()
            except:
                pass
    
    @staticmethod
    def _handle_peer(peer):
        try:
            # cmd_bytes y filename_bytes se leen como antes, no necesitan timestamp
            cmd_bytes = b''
            while not cmd_bytes.endswith(b'\0'):
                chunk = peer.recv(1)
                if not chunk: raise ConnectionAbortedError("Peer disconnected")
                cmd_bytes += chunk
            cmd = cmd_bytes[:-1].decode()

            if cmd == "GET_FILE":
                filename_bytes = b''
                while not filename_bytes.endswith(b'\0'):
                    chunk = peer.recv(1)
                    if not chunk: raise ConnectionAbortedError("Peer disconnected while reading filename")
                    filename_bytes += chunk
                filename = filename_bytes[:-1].decode()
                
                try:
                    file_size = os.path.getsize(filename)
                    with open(filename, 'rb') as f:
                        peer.sendall(bytes([client.RC.OK.value])) 
                        peer.sendall(str(file_size).encode() + b'\0')
                        bytes_sent = 0
                        while bytes_sent < file_size:
                            buf = f.read(1024)
                            if not buf: break
                            peer.sendall(buf)
                            bytes_sent += len(buf)
                except FileNotFoundError:
                    peer.sendall(bytes([client.RC.P2P_FILE_NOT_FOUND.value]))
                except Exception:
                    try:
                        peer.sendall(bytes([client.RC.P2P_OTHER_ERROR.value]))
                    except: pass
        except ConnectionAbortedError: pass
        finally:
            try: peer.close()
            except: pass

    @staticmethod
    def _stop_peer():
        client._stop = True
        temp_sock = client._listen_sock
        client._listen_sock = None
        if temp_sock:
            try: temp_sock.shutdown(socket.SHUT_RDWR)
            except OSError: pass
            finally:
                try: temp_sock.close()
                except OSError: pass

    # ---------------- REGISTER / UNREGISTER ----------------
    @staticmethod
    def register(user: str) -> "client.RC":
        timestamp = client._get_current_timestamp()
        if "ERROR" in timestamp or "UNAVAILABLE" in timestamp:
            print(f"REGISTER FAIL (timestamp service error: {timestamp})")
            return client.RC.ERROR
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((client._server, client._port))
                message = f"REGISTER\0{timestamp}\0{user}\0".encode('utf-8')
                s.sendall(message)
                response = s.recv(1)
                if not response:
                    print("REGISTER FAIL (no response)"); return client.RC.ERROR
                code = response[0]

            if code == client.RC.OK.value:
                print("REGISTER OK"); return client.RC.OK
            elif code == client.RC.USER_ERROR.value:
                print("USERNAME IN USE"); return client.RC.USER_ERROR
            else:
                print("REGISTER FAIL"); return client.RC.ERROR
        except socket.error:
            print("REGISTER FAIL (connection error)"); return client.RC.ERROR
        except Exception as e:
            print(f"REGISTER FAIL (unknown error: {e})"); return client.RC.ERROR

    @staticmethod
    def unregister(user: str) -> "client.RC":
        timestamp = client._get_current_timestamp()
        if "ERROR" in timestamp or "UNAVAILABLE" in timestamp:
            print(f"UNREGISTER FAIL (timestamp service error: {timestamp})")
            return client.RC.ERROR
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
                s.connect((client._server, client._port))
                message = f"UNREGISTER\0{timestamp}\0{user}\0".encode('utf-8')
                s.sendall(message)
                response = s.recv(1)
                if not response:
                    print("UNREGISTER FAIL (no response)"); return client.RC.ERROR
                code = response[0]

            if code == client.RC.OK.value:
                print("UNREGISTER OK"); return client.RC.OK
            elif code == client.RC.USER_ERROR.value:
                print("USER DOES NOT EXIST"); return client.RC.USER_ERROR
            else:
                print("UNREGISTER FAIL"); return client.RC.ERROR
        except socket.error:
            print("UNREGISTER FAIL (connection error)"); return client.RC.ERROR
        except Exception as e:
            print(f"UNREGISTER FAIL (unknown error: {e})"); return client.RC.ERROR

    # ---------------- CONNECT / DISCONNECT ----------------
    @staticmethod
    def connect(user: str) -> "client.RC":
        if client._sock is not None:
            print("ALREADY CONNECTED TO A SERVER. DISCONNECT FIRST OR USE A DIFFERENT INSTANCE."); return client.RC.USER_ERROR

        timestamp = client._get_current_timestamp()
        if "ERROR" in timestamp or "UNAVAILABLE" in timestamp:
            print(f"CONNECT FAIL (timestamp service error: {timestamp})")
            return client.RC.ERROR

        try:
            listen_sock_temp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            listen_sock_temp.bind(('', 0))
            port_local = listen_sock_temp.getsockname()[1]
        except socket.error:
            print("CONNECT FAIL (could not create listening socket)"); return client.RC.ERROR
        
        client._listen_sock = listen_sock_temp
        client._stop = False
        threading.Thread(target=client._serve_peer, args=(client._listen_sock,), daemon=True).start()

        sock_temp = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        code_val = client.RC.ERROR.value 

        try:
            sock_temp.connect((client._server, client._port))
            message = f"CONNECT\0{timestamp}\0{user}\0{port_local}\0".encode('utf-8')
            sock_temp.sendall(message)
            response = sock_temp.recv(1)
            if not response:
                raise socket.error("No response from server on CONNECT")
            code_val = response[0]

            if code_val == client.RC.OK.value:
                client._sock = sock_temp
                client._username = user
                print("CONNECT OK")
                return client.RC.OK
            elif code_val == 1:
                print("CONNECT FAIL, USER DOES NOT EXIST")
            elif code_val == 2:
                print("USER ALREADY CONNECTED")
            else:
                print("CONNECT FAIL")
            
            sock_temp.close()
            client._stop_peer()
            return client.RC.USER_ERROR if code_val in (1,2) else client.RC.ERROR # Ajustado
        
        except socket.error:
            print("CONNECT FAIL (connection error)")
            try: sock_temp.close()
            except: pass
            client._stop_peer()
            return client.RC.ERROR
        except Exception as e:
            print(f"CONNECT FAIL (unknown error: {e})")
            try: sock_temp.close()
            except: pass
            client._stop_peer()
            return client.RC.ERROR

    @staticmethod
    def disconnect() -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR
        
        s = client._sock
        username_to_send = client._username 
        return_code = client.RC.ERROR

        timestamp = client._get_current_timestamp() # Obtener timestamp para DISCONNECT
        if "ERROR" in timestamp or "UNAVAILABLE" in timestamp:
            print(f"DISCONNECT FAIL (timestamp service error: {timestamp})")
            # Continuar con la desconexión local como indica el PDF
        # else: # Solo enviar al servidor si el timestamp se obtuvo
            # No, el PDF dice que el cliente se desconecta localmente de todas formas.
            # La pregunta es si se *intenta* enviar el comando al servidor aun con error de TS.
            # Por ahora, lo intentaremos, el servidor podría rechazarlo si el TS es inválido.
            # O podríamos decidir no enviarlo y solo hacer limpieza local.
            # Para ser consistentes, enviamos el comando (con el TS de error si es el caso).

        try:
            # Protocolo: DISCONNECT\0<timestamp>\0<user_name>\0
            message = f"DISCONNECT\0{timestamp}\0{username_to_send}\0".encode('utf-8')
            s.sendall(message)
            response = s.recv(1)
            if not response:
                raise socket.error("No response from server on DISCONNECT")
            
            code_val = response[0]
            if code_val == client.RC.OK.value:
                print("DISCONNECT OK")
                return_code = client.RC.OK
            elif code_val == 1:
                print("DISCONNECT FAIL, USER DOES NOT EXIST")
                return_code = client.RC.USER_ERROR
            elif code_val == 2:
                print("DISCONNECT FAIL, USER NOT CONNECTED")
                return_code = client.RC.USER_ERROR
            else:
                print("DISCONNECT FAIL")
                return_code = client.RC.ERROR

        except socket.error:
            print("DISCONNECT FAIL (connection error)")
            return_code = client.RC.ERROR 
        except Exception as e:
            print(f"DISCONNECT FAIL (unknown error: {e})")
            return_code = client.RC.ERROR
        finally:
            if s:
                try: s.close()
                except: pass
            client._sock = None
            client._username = None
            client._stop_peer()
        return return_code

    # ---------------- PUBLISH ----------------
    @staticmethod
    def publish(file_name: str, desc: str) -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR
        
        timestamp = client._get_current_timestamp()
        if "ERROR" in timestamp or "UNAVAILABLE" in timestamp:
            print(f"PUBLISH FAIL (timestamp service error: {timestamp})")
            return client.RC.ERROR

        s = client._sock
        try:
            # Protocolo: PUBLISH\0<timestamp>\0<user_name>\0<file_name>\0<description>\0
            message = f"PUBLISH\0{timestamp}\0{client._username}\0{file_name}\0{desc}\0".encode('utf-8')
            s.sendall(message)
            response = s.recv(1)
            if not response: raise socket.error("No response on PUBLISH")
            code = response[0]

            if code == client.RC.OK.value:
                print("PUBLISH OK"); return client.RC.OK
            elif code == 1:
                print("PUBLISH FAIL, USER DOES NOT EXIST"); return client.RC.USER_ERROR
            elif code == 2:
                print("PUBLISH FAIL, USER NOT CONNECTED"); return client.RC.USER_ERROR
            elif code == 3:
                print("PUBLISH FAIL, CONTENT ALREADY PUBLISHED"); return client.RC.USER_ERROR
            else:
                print("PUBLISH FAIL"); return client.RC.ERROR
        except socket.error:
            print("PUBLISH FAIL (connection error)"); return client.RC.ERROR
        except Exception as e:
            print(f"PUBLISH FAIL (unknown error: {e})"); return client.RC.ERROR

    # ---------------- DELETE ----------------
    @staticmethod
    def delete(file_name: str) -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR

        timestamp = client._get_current_timestamp()
        if "ERROR" in timestamp or "UNAVAILABLE" in timestamp:
            print(f"DELETE FAIL (timestamp service error: {timestamp})")
            return client.RC.ERROR
            
        s = client._sock
        try:
            # Protocolo: DELETE\0<timestamp>\0<user_name>\0<file_name>\0
            message = f"DELETE\0{timestamp}\0{client._username}\0{file_name}\0".encode('utf-8')
            s.sendall(message)
            response = s.recv(1)
            if not response: raise socket.error("No response on DELETE")
            code = response[0]

            if code == client.RC.OK.value:
                print("DELETE OK"); return client.RC.OK
            elif code == 1:
                print("DELETE FAIL, USER DOES NOT EXIST"); return client.RC.USER_ERROR
            elif code == 2:
                print("DELETE FAIL, USER NOT CONNECTED"); return client.RC.USER_ERROR
            elif code == 3:
                print("DELETE FAIL, CONTENT NOT PUBLISHED"); return client.RC.USER_ERROR
            else:
                print("DELETE FAIL"); return client.RC.ERROR
        except socket.error:
            print("DELETE FAIL (connection error)"); return client.RC.ERROR
        except Exception as e:
            print(f"DELETE FAIL (unknown error: {e})"); return client.RC.ERROR

    # ---------------- LIST_USERS ----------------
    @staticmethod
    def list_users() -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR

        timestamp = client._get_current_timestamp()
        if "ERROR" in timestamp or "UNAVAILABLE" in timestamp:
            print(f"LIST_USERS FAIL (timestamp service error: {timestamp})")
            return client.RC.ERROR

        s = client._sock
        try:
            # Protocolo: LIST_USERS\0<timestamp>\0<user_name>\0
            message = f"LIST_USERS\0{timestamp}\0{client._username}\0".encode('utf-8')
            s.sendall(message)
            response = s.recv(1)
            if not response: raise socket.error("No response on LIST_USERS")
            code = response[0]

            if code != client.RC.OK.value:
                if code == 1: print("LIST_USERS FAIL, USER DOES NOT EXIST")
                elif code == 2: print("LIST_USERS FAIL, USER NOT CONNECTED")
                else: print("LIST_USERS FAIL")
                return client.RC.USER_ERROR if code in (1,2) else client.RC.ERROR

            data = b''
            while not data.endswith(b'\0'):
                chunk = s.recv(1)
                if not chunk: raise socket.error("Connection lost while reading count for LIST_USERS")
                data += chunk
            count = int(data[:-1].decode())
            
            client._peers.clear()
            print("LIST_USERS OK")
            for _ in range(count):
                nb = b''
                while True:
                    c = s.recv(1);
                    if not c: raise socket.error("Connection lost")
                    if c == b'\0': break
                    nb += c
                name = nb.decode()
                ib = b''
                while True:
                    c = s.recv(1);
                    if not c: raise socket.error("Connection lost")
                    if c == b'\0': break
                    ib += c
                ip = ib.decode()
                pb = b''
                while True:
                    c = s.recv(1);
                    if not c: raise socket.error("Connection lost")
                    if c == b'\0': break
                    pb += c
                port = int(pb.decode())
                client._peers[name] = (ip, port)
                print(f"{name}\t{ip}\t{port}")
            return client.RC.OK
        except socket.error:
            print("LIST_USERS FAIL (connection error)"); return client.RC.ERROR
        except Exception as e:
            print(f"LIST_USERS FAIL (unknown error: {e})"); return client.RC.ERROR

    # ---------------- LIST_CONTENT ----------------
    @staticmethod
    def list_content(target_user: str) -> "client.RC":
        if client._sock is None:
            print("USER NOT CONNECTED"); return client.RC.USER_ERROR

        timestamp = client._get_current_timestamp()
        if "ERROR" in timestamp or "UNAVAILABLE" in timestamp:
            print(f"LIST_CONTENT FAIL (timestamp service error: {timestamp})")
            return client.RC.ERROR

        s = client._sock
        try:
            # Protocolo: LIST_CONTENT\0<timestamp>\0<user_name_operacion>\0<user_name_contenido>\0
            message = f"LIST_CONTENT\0{timestamp}\0{client._username}\0{target_user}\0".encode('utf-8')
            s.sendall(message)
            response = s.recv(1)
            if not response: raise socket.error("No response on LIST_CONTENT")
            code = response[0]

            if code != client.RC.OK.value:
                if code == 1: print("LIST_CONTENT FAIL, USER DOES NOT EXIST")
                elif code == 2: print("LIST_CONTENT FAIL, USER NOT CONNECTED")
                elif code == 3: print("LIST_CONTENT FAIL, REMOTE USER DOES NOT EXIST")
                else: print("LIST_CONTENT FAIL")
                return client.RC.USER_ERROR if code in (1,2,3) else client.RC.ERROR

            data = b''
            while not data.endswith(b'\0'):
                chunk = s.recv(1)
                if not chunk: raise socket.error("Connection lost while reading count for LIST_CONTENT")
                data += chunk
            count = int(data[:-1].decode())
            
            print("LIST_CONTENT OK")
            for _ in range(count):
                fb = b''
                while True:
                    c = s.recv(1)
                    if not c: raise socket.error("Connection lost")
                    if c == b'\0': break
                    fb += c
                print(fb.decode())
            return client.RC.OK
        except socket.error:
            print("LIST_CONTENT FAIL (connection error)"); return client.RC.ERROR
        except Exception as e:
            print(f"LIST_CONTENT FAIL (unknown error: {e})"); return client.RC.ERROR

    # ---------------- GET_FILE ----------------
    # GET_FILE es una comunicación P2P, no pasa por el servidor central de Parte 1,
    # por lo tanto, no necesita enviar el timestamp del servicio web a otro peer.
    # El protocolo para GET_FILE entre peers se mantiene como en la Parte 1.
    @staticmethod
    def get_file(remote_user: str, remote_file: str, local_file: str) -> "client.RC":
        if client._username is None: 
            print("GET_FILE FAIL (client not connected to central server)"); return client.RC.USER_ERROR

        if remote_user not in client._peers:
            print(f"Info for {remote_user} not in cache. Fetching user list...")
            res = client.list_users() 
            if res != client.RC.OK or remote_user not in client._peers:
                print(f"GET_FILE FAIL, REMOTE USER {remote_user} DOES NOT EXIST or info not available")
                return client.RC.USER_ERROR

        ip, port = client._peers[remote_user]
        peer_socket = None
        file_handle = None
        operation_failed = False
        bytes_received = 0 # Definir fuera para el finally
        file_size = -1     # Definir fuera para el finally

        try:
            peer_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            peer_socket.connect((ip, port))
            peer_socket.sendall(b"GET_FILE\0" + remote_file.encode() + b'\0')
            
            response_code_byte = peer_socket.recv(1)
            if not response_code_byte:
                print("GET_FILE FAIL (no response from peer)")
                operation_failed = True; return client.RC.ERROR
            
            peer_response_code = response_code_byte[0]

            if peer_response_code == client.RC.OK.value:
                size_str_bytes = b''
                while not size_str_bytes.endswith(b'\0'):
                    chunk = peer_socket.recv(1)
                    if not chunk: raise socket.error("Peer disconnected while sending file size")
                    size_str_bytes += chunk
                file_size = int(size_str_bytes[:-1].decode())

                file_handle = open(local_file, 'wb')
                # bytes_received = 0 # Ya está definido arriba
                while bytes_received < file_size:
                    chunk = peer_socket.recv(min(1024, file_size - bytes_received))
                    if not chunk:
                        print("GET_FILE FAIL (peer disconnected during transfer)")
                        operation_failed = True; return client.RC.ERROR
                    file_handle.write(chunk)
                    bytes_received += len(chunk)
                
                file_handle.close(); file_handle = None
                print("GET_FILE OK"); return client.RC.OK
            elif peer_response_code == client.RC.P2P_FILE_NOT_FOUND.value:
                print("GET_FILE FAIL, FILE NOT EXIST (on peer)")
                return client.RC.USER_ERROR 
            else:
                print("GET_FILE FAIL (peer reported an error)")
                return client.RC.ERROR
        except socket.error as e:
            print(f"GET_FILE FAIL (socket error: {e})")
            operation_failed = True; return client.RC.ERROR
        except Exception as e:
            print(f"GET_FILE FAIL (unknown error: {e})")
            operation_failed = True; return client.RC.ERROR
        finally:
            if peer_socket:
                try: peer_socket.close()
                except: pass
            if file_handle: # Si se abrió y no se cerró por éxito
                try: file_handle.close()
                except: pass
            # Solo borrar si la operación falló Y se creó un fichero Y no se completó
            if operation_failed and os.path.exists(local_file) and (file_size == -1 or (bytes_received > 0 and bytes_received < file_size)):
                try:
                    os.remove(local_file)
                    print(f"Local file {local_file} removed due to incomplete transfer.")
                except OSError:
                    print(f"Could not remove incomplete local file {local_file}")

    # ---------------- Shell ----------------
    @staticmethod
    def shell():
        while True:
            try:
                line = input("c> ")
            except EOFError:
                print(); break
            if not line.strip(): continue
            parts = line.strip().split()
            cmd = parts[0].upper()
            args = parts[1:]

            if cmd in ("EXIT", "QUIT"):
                if client._sock: client.disconnect()
                client._stop_peer()
                break
            elif cmd == "REGISTER"   and len(args) == 1: client.register(args[0])
            elif cmd == "UNREGISTER" and len(args) == 1: client.unregister(args[0])
            elif cmd == "CONNECT"    and len(args) == 1:
                if client._username is not None and client._username != args[0]:
                    print(f"Disconnecting current user {client._username} before connecting as {args[0]}.")
                    client.disconnect()
                if client._username is None:
                    client.connect(args[0])
                elif client._username == args[0] and client._sock is not None:
                     print("USER ALREADY CONNECTED")
            elif cmd == "PUBLISH"    and len(args) >= 2:
                fn, desc = args[0], " ".join(args[1:])
                client.publish(fn, desc)
            elif cmd == "DELETE"     and len(args) == 1: client.delete(args[0])
            elif cmd == "LIST_USERS" and len(args) == 0: client.list_users()
            elif cmd == "LIST_CONTENT" and len(args) == 1: client.list_content(args[0])
            elif cmd == "GET_FILE"   and len(args) == 3: client.get_file(args[0], args[1], args[2])
            elif cmd == "DISCONNECT" and len(args) == 0: client.disconnect()
            else: print("Sintaxis inválida")
        print("Cliente terminado.")

    @staticmethod
    def main(argv):
        if not client.parseArguments(argv):
            client.usage(); return
        try: client.shell()
        finally:
            if not client._stop : client._stop_peer()

if __name__ == "__main__":
    client.main(sys.argv[1:])