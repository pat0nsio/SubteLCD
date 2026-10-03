import os
import time
import requests
from dotenv import load_dotenv

load_dotenv()

CLIENT_ID = os.getenv("CLIENT_ID")
CLIENT_SECRET = os.getenv("CLIENT_SECRET")

# Tiempo medio entre estaciones, para estimar paradas sin pronóstico. Calibrar mirando trenes reales.
SEG_POR_ESTACION = 110


def es_relleno(arrival, ts):
    # La API rellena las paradas sin pronóstico con arrival ≈ Header.timestamp (0 o -1 s).
    return arrival == 0 or abs(arrival - ts) <= 1


def estimar_llegada(t, k, ts):
    # Interpola entre las paradas vecinas con dato real, o extrapola con SEG_POR_ESTACION
    # si hay dato de un solo lado. None = el viaje no tiene datos.
    # ponytail: asume tramos de igual duración; usar tiempos por tramo del GTFS estático si hace falta precisión.
    antes = [i for i in range(k) if not es_relleno(t[i], ts)]
    despues = [j for j in range(k + 1, len(t)) if not es_relleno(t[j], ts)]
    if antes and despues:
        i, j = antes[-1], despues[0]
        return t[i] + (t[j] - t[i]) * (k - i) // (j - i)
    if antes:
        return t[antes[-1]] + (k - antes[-1]) * SEG_POR_ESTACION
    if despues:
        return t[despues[0]] - (despues[0] - k) * SEG_POR_ESTACION
    return None


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
    ts = data.get("Header", {}).get("timestamp", 0)
    candidatos = []

    if "Entity" in data:
        for tren in data["Entity"]:
            info_linea = tren.get("Linea", {})

            if (info_linea.get("Route_Id") == OBJETIVO_LINEA and
                    info_linea.get("Direction_ID") == OBJETIVO_DIRECCION):

                estaciones = info_linea.get("Estaciones", [])
                nombres = [p["stop_name"] for p in estaciones]
                if OBJETIVO_ESTACION not in nombres:
                    continue

                k = nombres.index(OBJETIVO_ESTACION)
                tiempos = [p.get("arrival", {}).get("time", 0) for p in estaciones]

                tiempo_llegada = tiempos[k]
                estimado = es_relleno(tiempo_llegada, ts)
                if estimado:
                    tiempo_llegada = estimar_llegada(tiempos, k, ts)
                if tiempo_llegada is None:
                    continue

                segundos_restantes = tiempo_llegada - hora_actual
                if segundos_restantes > 0:
                    candidatos.append((segundos_restantes, estimado))

    candidatos.sort()

    print(f"\n--- Próximos trenes en {OBJETIVO_ESTACION} (Sentido Catedral) ---")

    if not candidatos:
        print("No hay trenes aproximándose (todos están en andén o pasaron).")
    else:
        for i, (segundos, estimado) in enumerate(candidatos[:3]):
            minutos = int(segundos // 60)
            segs = int(segundos % 60)
            print(f"{i+1}. Llega en: {'~' if estimado else ''}{minutos} min {segs} seg")

if __name__ == "__main__":
    main()
