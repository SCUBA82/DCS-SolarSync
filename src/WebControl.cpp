#include "WebControl.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include "WebUI.h"

// Ist die Sendewarteschlange laenger, antwortet der Server mit "busy",
// und die Webseite sendet den letzten Wert spaeter erneut.
static const size_t BUSY_QUEUE_SIZE = 8;

WebControl::WebControl(BRMesh &mesh, SceneConfig &scene, uint16_t port)
    : mesh_(mesh), scene_(scene), server_(port), port_(port) {}

void WebControl::begin() {
  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/api/state", HTTP_GET, [this]() { handleState(); });
  server_.on("/api/set", HTTP_GET, [this]() { handleSet(); });
  server_.on("/api/auto", HTTP_GET, [this]() { handleAuto(); });
  server_.on("/api/scene", HTTP_GET, [this]() { handleSceneGet(); });
  server_.on("/api/scene", HTTP_POST, [this]() { handleScenePost(); });
  server_.on("/api/scene/reset", HTTP_POST, [this]() { handleSceneReset(); });
  server_.on("/api/live", HTTP_GET, [this]() { handleLive(); });
}

void WebControl::loop() {
  if (!started_) {
    if (WiFi.status() != WL_CONNECTED) return;
    server_.begin();
    started_ = true;
    Serial.printf("WLAN-Hostname: %s\n", WiFi.getHostname());
    Serial.printf("Webinterface: http://%s/\n", WiFi.localIP().toString().c_str());

    if (hostname_.length() > 0) {
      if (MDNS.begin(hostname_.c_str())) {
        MDNS.addService("http", "tcp", port_);
        Serial.printf("mDNS: http://%s.local/\n", hostname_.c_str());
      } else {
        Serial.println("mDNS konnte nicht gestartet werden.");
      }
    }
  }
  server_.handleClient();
}

int WebControl::argInt(const char *name, int defaultValue, int minValue, int maxValue) {
  if (!server_.hasArg(name)) return defaultValue;
  int value = server_.arg(name).toInt();
  if (value < minValue) value = minValue;
  if (value > maxValue) value = maxValue;
  return value;
}

void WebControl::handleRoot() {
  // send_P streamt direkt aus dem Flash. send() wuerde die ~30 KB zuerst in einen
  // String kopieren, was bei knappem Heap fehlschlagen kann (leere Seite).
  Serial.printf("Webseite: freier Heap %u, groesster Block %u\n",
                (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
  server_.send_P(200, "text/html; charset=utf-8", INDEX_HTML);
}

void WebControl::sendState(int lamp) {
  if (lamp < 1) lamp = 1;  // "Alle": Werte von Lampe 1 anzeigen

  char json[220];
  snprintf(json, sizeof(json),
           "{\"lamp\":%d,\"lamps\":%u,\"r\":%u,\"g\":%u,\"b\":%u,\"br\":%u,"
           "\"w6\":%u,\"w7\":%u,\"auto\":%s,\"queue\":%u}",
           lamp, mesh_.lampCount(),
           mesh_.red(lamp), mesh_.green(lamp), mesh_.blue(lamp),
           mesh_.brightness(lamp), mesh_.white6(lamp), mesh_.white7(lamp),
           manual_ ? "false" : "true", (unsigned)mesh_.queueSize());

  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", json);
}

void WebControl::handleState() {
  sendState(argInt("lamp", 1, 0, mesh_.lampCount()));
}

void WebControl::handleSet() {
  if (mesh_.queueSize() >= BUSY_QUEUE_SIZE) {
    server_.send(429, "application/json", "{\"busy\":true}");
    return;
  }

  const int lamp = argInt("lamp", 0, 0, mesh_.lampCount());
  const uint8_t r  = argInt("r", 0, 0, 255);
  const uint8_t g  = argInt("g", 0, 0, 255);
  const uint8_t b  = argInt("b", 0, 0, 255);
  const uint8_t br = argInt("br", 255, 0, 255);
  const uint8_t w6 = argInt("w6", 0, 0, 255);
  const uint8_t w7 = argInt("w7", 0, 0, 255);

  manual_ = true;  // Handeingriff stoppt die Automatik

  if (lamp == 0) {
    mesh_.setAll(r, g, b, br, 0, w6, w7);
  } else {
    mesh_.setLamp(lamp, r, g, b, br, w6, w7);
  }

  sendState(lamp);
}

void WebControl::handleAuto() {
  const bool on = argInt("on", 0, 0, 1) == 1;
  const bool wasManual = manual_;
  manual_ = !on;

  if (on && wasManual && autoResumeCallback_) autoResumeCallback_();

  sendState(1);
}

void WebControl::handleSceneGet() {
  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", scene_.toJson());
}

void WebControl::handleScenePost() {
  String error;
  const bool ok = scene_.update(server_.arg("switch"), server_.arg("hyst"),
                                server_.arg("rgb"), server_.arg("white"), error);
  if (!ok) {
    server_.send(400, "application/json", "{\"ok\":false,\"error\":\"" + error + "\"}");
    return;
  }

  // Bei aktiver Automatik die neue Szene sofort senden
  if (!manual_ && autoResumeCallback_) autoResumeCallback_();
  server_.send(200, "application/json", "{\"ok\":true}");
}

void WebControl::handleSceneReset() {
  scene_.resetDefaults();
  const bool saved = scene_.save();
  if (!manual_ && autoResumeCallback_) autoResumeCallback_();
  server_.send(200, "application/json", saved ? "{\"ok\":true}" : "{\"ok\":false,\"error\":\"Saving failed\"}");
}

void WebControl::handleLive() {
  String j;
  j.reserve(900);
  j += "{\"manual\":";
  j += manual_ ? "true" : "false";
  j += ",\"queue\":";
  j += (unsigned)mesh_.queueSize();

  // Zuletzt an die Lampen gesendete Werte
  j += ",\"lamps\":[";
  for (int lamp = 1; lamp <= mesh_.lampCount(); lamp++) {
    if (lamp > 1) j += ',';
    j += "{\"r\":";  j += (unsigned)mesh_.red(lamp);
    j += ",\"g\":";  j += (unsigned)mesh_.green(lamp);
    j += ",\"b\":";  j += (unsigned)mesh_.blue(lamp);
    j += ",\"br\":"; j += (unsigned)mesh_.brightness(lamp);
    j += ",\"w6\":"; j += (unsigned)mesh_.white6(lamp);
    j += ",\"w7\":"; j += (unsigned)mesh_.white7(lamp);
    j += '}';
  }
  j += ']';

  j += ",\"data\":";
  j += liveCallback_ ? liveCallback_() : String("{}");
  j += '}';

  server_.sendHeader("Cache-Control", "no-store");
  server_.send(200, "application/json", j);
}
