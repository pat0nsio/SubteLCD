
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal.h>

// ==========================================================
//   CONFIGURACIÓN (equivalente al .env + variables del script)
// ==========================================================
const char* WIFI_SSID     = "TU_WIFI";
const char* WIFI_PASSWORD = "TU_PASSWORD";

const char* CLIENT_ID     = "e82491b995494b9abb5a571f59cf5c05";
const char* CLIENT_SECRET = "26F8f2c490754E7A8765A7e1EBA5b31c";

// Objetivo (fijo, como en el script):
const char* OBJETIVO_LINEA     = "LineaD";
const char* OBJETIVO_ESTACION  = "Bulnes";
const int   OBJETIVO_DIRECCION = 1;            // Hacia Catedral
const char* TEXTO_SENTIDO      = "Catedral";   // Rótulo manual del sentido

const unsigned long INTERVALO_CONSULTA_MS = 30000UL; // re-consulta a la API cada 30 s

// ==========================================================
//   LCD 2004 en paralelo (4 bits)
// ==========================================================
const int LCD_RS = 22;
const int LCD_E  = 23;
const int LCD_D4 = 5;
const int LCD_D5 = 18;
const int LCD_D6 = 19;
const int LCD_D7 = 21;

const int LCD_COLS = 20;
const int LCD_ROWS = 4;

LiquidCrystal lcd(LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);

const char* API_HOST = "apitransporte.buenosaires.gob.ar";
const char* API_PATH = "/subtes/forecastGTFS";

// ==========================================================
//   Estado global
// ==========================================================
const int MAX_CANDIDATOS = 8;
long  candidatos[MAX_CANDIDATOS];   // segundos restantes (ordenados) de la última consulta
int   numCandidatos = 0;
bool  consultaValida = false;

unsigned long ultimaConsultaMs = 0;   // millis() de la última consulta a la API
unsigned long ultimoRefrescoMs = 0;   // millis() del último refresco del LCD

// ----------------------------------------------------------
//   Utilidades de LCD
// ----------------------------------------------------------

// Escribe una línea completa rellenando con espacios hasta LCD_COLS (evita fantasmas).
void lcdLinea(int fila, const String& texto) {
  String s = texto;
  if (s.length() > (unsigned)LCD_COLS) s = s.substring(0, LCD_COLS);
  while (s.length() < (unsigned)LCD_COLS) s += ' ';
  lcd.setCursor(0, fila);
  lcd.print(s);
}

// Formatea segundos a "m min s s" o estados especiales.
String formatTiempo(long segundos) {
  if (segundos <= 0) {
    return "En anden / Llega ya";
  }
  long minutos = segundos / 60;
  long segs    = segundos % 60;
  return String(minutos) + " min " + String(segs) + " s";
}

// ----------------------------------------------------------
//   WiFi
// ----------------------------------------------------------
void conectarWiFi() {
  lcdLinea(0, "Subte LCD");
  lcdLinea(1, "Conectando WiFi...");
  lcdLinea(2, WIFI_SSID);
  lcdLinea(3, "");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long inicio = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - inicio < 20000UL) {
    delay(250);
    Serial.print('.');
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println();
    Serial.print("WiFi OK, IP: ");
    Serial.println(WiFi.localIP());
    lcdLinea(1, "WiFi conectado");
    lcdLinea(2, WiFi.localIP().toString());
    delay(800);
  } else {
    Serial.println("\nFallo WiFi");
    lcdLinea(1, "Fallo WiFi");
  }
}


bool consultarAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
    if (WiFi.status() != WL_CONNECTED) return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = String("https://") + API_HOST + API_PATH +
               "?client_id=" + CLIENT_ID +
               "&client_secret=" + CLIENT_SECRET;

  Serial.println("GET " + String(API_PATH));
  if (!http.begin(client, url)) {
    Serial.println("http.begin() falló");
    return false;
  }

  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("HTTP error: %d\n", code);
    http.end();
    return false;
  }

  JsonDocument filtro;
  filtro["Header"]["timestamp"] = true;
  JsonObject fLinea = filtro["Entity"][0]["Linea"].to<JsonObject>();
  fLinea["Route_Id"]     = true;
  fLinea["Direction_ID"] = true;
  JsonObject fEst = fLinea["Estaciones"][0].to<JsonObject>();
  fEst["stop_name"]       = true;
  fEst["arrival"]["time"] = true;

  JsonDocument doc;
  DeserializationError err = deserializeJson(
      doc, http.getStream(), DeserializationOption::Filter(filtro));
  http.end();

  if (err) {
    Serial.print("deserializeJson falló: ");
    Serial.println(err.c_str());
    return false;
  }

  long horaReferencia = doc["Header"]["timestamp"] | 0L;
  if (horaReferencia == 0) {
    Serial.println("Sin Header.timestamp");
    return false;
  }

  numCandidatos = 0;

  for (JsonObject tren : doc["Entity"].as<JsonArray>()) {
    JsonObject linea = tren["Linea"];
    if (linea.isNull()) continue;

    const char* routeId   = linea["Route_Id"]     | "";
    int         direction = linea["Direction_ID"] | -999;

    if (strcmp(routeId, OBJETIVO_LINEA) != 0) continue;
    if (direction != OBJETIVO_DIRECCION) continue;

    for (JsonObject parada : linea["Estaciones"].as<JsonArray>()) {
      const char* stopName = parada["stop_name"] | "";

      if (strstr(stopName, OBJETIVO_ESTACION) == nullptr) continue;

      long arrival = parada["arrival"]["time"] | 0L;
      if (arrival == 0) break;

      long restantes = arrival - horaReferencia;
      if (restantes > -30 && numCandidatos < MAX_CANDIDATOS) {
        candidatos[numCandidatos++] = restantes;
      }
      break;
    }
  }

  for (int i = 1; i < numCandidatos; i++) {
    long v = candidatos[i];
    int j = i - 1;
    while (j >= 0 && candidatos[j] > v) {
      candidatos[j + 1] = candidatos[j];
      j--;
    }
    candidatos[j + 1] = v;
  }

  Serial.printf("Candidatos: %d  (heap libre: %u)\n", numCandidatos, ESP.getFreeHeap());
  for (int i = 0; i < numCandidatos; i++) Serial.printf("  %ld s\n", candidatos[i]);

  return true;
}


void mostrarEnLCD(unsigned long transcurridoMs) {

  String lineaCorta = String(OBJETIVO_LINEA);
  lineaCorta.replace("Linea", "Linea ");  // "LineaD" -> "Linea D"
  lcdLinea(0, lineaCorta + " - " + OBJETIVO_ESTACION);
  lcdLinea(1, String("Sentido: ") + TEXTO_SENTIDO);

  if (!consultaValida) {
    lcdLinea(2, "Error de conexion");
    lcdLinea(3, "Reintentando...");
    return;
  }

  if (numCandidatos == 0) {
    lcdLinea(2, "Sin trenes ahora");
    lcdLinea(3, "");
    return;
  }

  long offset = (long)(transcurridoMs / 1000UL);

  long t1 = candidatos[0] - offset;
  lcdLinea(2, String("1) ") + formatTiempo(t1));

  if (numCandidatos > 1) {
    long t2 = candidatos[1] - offset;
    lcdLinea(3, String("2) ") + formatTiempo(t2));
  } else {
    lcdLinea(3, "");
  }
}


void setup() {
  Serial.begin(115200);
  delay(200);

  lcd.begin(LCD_COLS, LCD_ROWS);
  lcd.clear();

  conectarWiFi();

  consultaValida = consultarAPI();
  ultimaConsultaMs = millis();
  ultimoRefrescoMs = millis();
  mostrarEnLCD(0);
}

void loop() {
  unsigned long ahora = millis();

  // Re-consultar la API cada INTERVALO_CONSULTA_MS.
  if (ahora - ultimaConsultaMs >= INTERVALO_CONSULTA_MS) {
    consultaValida = consultarAPI();
    ultimaConsultaMs = millis();
    mostrarEnLCD(0);
    ultimoRefrescoMs = millis();
    return;
  }

  if (ahora - ultimoRefrescoMs >= 1000UL) {
    ultimoRefrescoMs = ahora;
    mostrarEnLCD(ahora - ultimaConsultaMs);
  }
}
