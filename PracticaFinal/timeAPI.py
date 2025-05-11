from flask import Flask, Response
import datetime
import time

app = Flask(__name__)

# Define el puerto y host
# HOST='localhost' para que solo sea accesible desde la máquina local
# PORT puede ser el que desees, ej. 8000 o 5000 (puerto común para Flask)
HOST = 'localhost'
PORT = 5000 # Puedes cambiarlo si es necesario

@app.route('/getTime', methods=['GET'])
def get_time():
    # Obtener fecha y hora actual en el formato DD/MM/YYYY HH:MM:SS
    now = datetime.datetime.now()
    timestamp_str = now.strftime("%d/%m/%Y %H:%M:%S")
    
    # Devolver la cadena como texto plano
    return Response(timestamp_str, mimetype='text/plain')

def run_time_api():
    try:
        print(f"Servicio de hora REST (Flask) iniciado en http://{HOST}:{PORT}/getTime")
        print("Presiona Ctrl+C para detener el servidor.")
        app.run(host=HOST, port=PORT)
    except Exception as e:
        print(f"Error al iniciar el servidor de hora con Flask: {e}")

if __name__ == '__main__':
    run_time_api()