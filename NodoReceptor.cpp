// =============================
// ESP8266-Central.ino
// Nodo Central - Receptor y Servidor Web
// =============================
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <FS.h>
#include <ArduinoJson.h>

AsyncWebServer server(80);

// Estructura para guardar datos de cada luz
struct Luz {
  float lat;
  float lng;
  String estado;
};

std::vector<Luz> luces;

String getLucesJson() {
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (auto &luz : luces) {
    JsonObject obj = arr.createNestedObject();
    obj["lat"] = luz.lat;
    obj["lng"] = luz.lng;
    obj["estado"] = luz.estado;
  }

  String result;
  serializeJson(doc, result);
  return result;
}

void setup() {
  Serial.begin(115200);
  WiFi.begin("TU_SSID", "TU_PASSWORD");
  Serial.print("Conectando a WiFi");
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("\nError: No se pudo conectar a la red WiFi.");
    return;
  }

  Serial.println("\nWiFi conectado. IP: " + WiFi.localIP().toString());

  if (!SPIFFS.begin()) {
    Serial.println("Error al montar SPIFFS");
    return;
  }

  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.html");

  server.on("/estado-luces", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "application/json", getLucesJson());
  });

  server.on("/actualizar-luz", HTTP_POST, [](AsyncWebServerRequest *request){},
    NULL,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      StaticJsonDocument<256> doc;
      DeserializationError error = deserializeJson(doc, data);
      if (error) {
        Serial.println("Error al parsear JSON: " + String(error.c_str()));
        request->send(400, "text/plain", "JSON inválido");
        return;
      }

      if (!doc.containsKey("lat") || !doc.containsKey("lng") || !doc.containsKey("estado")) {
        request->send(400, "text/plain", "Faltan datos");
        return;
      }

      Luz nuevaLuz;
      nuevaLuz.lat = doc["lat"];
      nuevaLuz.lng = doc["lng"];
      nuevaLuz.estado = doc["estado"].as<String>();

      bool actualizado = false;
      for (auto &l : luces) {
        if (l.lat == nuevaLuz.lat && l.lng == nuevaLuz.lng) {
          l.estado = nuevaLuz.estado;
          actualizado = true;
          break;
        }
      }
      if (!actualizado) luces.push_back(nuevaLuz);

      request->send(200, "text/plain", "Luz actualizada");
    });

  server.onNotFound([](AsyncWebServerRequest *request){
    request->send(404, "text/plain", "Ruta no encontrada");
  });

  server.begin();
  Serial.println("Servidor iniciado");
}

void loop() {
  // Nada aquí, usamos AsyncWebServer
}
