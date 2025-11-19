import os
import time
import requests
from dotenv import load_dotenv

load_dotenv()

CLIENT_ID = os.getenv("CLIENT_ID")
CLIENT_SECRET = os.getenv("CLIENT_SECRET")

def main():
    url = "https://apitransporte.buenosaires.gob.ar/subtes/forecastGTFS"
    params = {
        "client_id": CLIENT_ID,
        "client_secret": CLIENT_SECRET
    }

    try:
        response = requests.get(url, params=params)
        response.raise_for_status()
        data = response.json()
    except Exception as e:
        print(f"Error de conexión: {e}")
        return

    OBJETIVO_LINEA = "LineaD"
    OBJETIVO_ESTACION = "Bulnes"
    OBJETIVO_DIRECCION = 1  # Hacia Catedral

    hora_actual = time.time()
    candidatos = []

    if "Entity" in data:
        for tren in data["Entity"]:
            info_linea = tren.get("Linea", {})

            if (info_linea.get("Route_Id") == OBJETIVO_LINEA and
                    info_linea.get("Direction_ID") == OBJETIVO_DIRECCION):

                for parada in info_linea.get("Estaciones", []):
                    if parada["stop_name"] == OBJETIVO_ESTACION:

                        tiempo_llegada = parada["arrival"]["time"]
                        segundos_restantes = tiempo_llegada - hora_actual

                        if segundos_restantes > 0:
                            candidatos.append(segundos_restantes)

                        break

    candidatos.sort()

    print(f"\n--- Próximos trenes en {OBJETIVO_ESTACION} (Sentido Catedral) ---")

    if not candidatos:
        print("No hay trenes aproximándose (todos están en andén o pasaron).")
    else:
        for i, segundos in enumerate(candidatos[:3]):
            minutos = int(segundos // 60)
            segs = int(segundos % 60)
            print(f"{i+1}. Llega en: {minutos} min {segs} seg")

if __name__ == "__main__":
    main()