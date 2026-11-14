#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <functional>

#include "BRMesh.h"
#include "SceneConfig.h"

/**
 * Kleines Webinterface zur manuellen Lampensteuerung (Farbrad, Helligkeit,
 * Warm-/Kaltweiss, Eingabefelder).
 *
 * Sobald ueber das Webinterface ein Wert gesetzt wird, wechselt die Steuerung
 * in den manuellen Modus (isManual() == true). Die Automatik laeuft erst wieder,
 * wenn sie im Webinterface eingeschaltet wird.
 */
class WebControl {
public:
  WebControl(BRMesh &mesh, SceneConfig &scene, uint16_t port = 80);

  // Routen registrieren (der Server startet automatisch, sobald das WLAN verbunden ist)
  void begin();

  // In loop() aufrufen
  void loop();

  // Hostname fuer mDNS (erreichbar als http://<hostname>.local/).
  // Den WLAN-Hostnamen selbst setzt man mit WiFi.setHostname() VOR dem Verbinden.
  void setHostname(const char *hostname) { hostname_ = hostname; }

  bool isManual() const { return manual_; }
  void setManual(bool manual) { manual_ = manual; }

  // Wird aufgerufen, wenn die Automatik wieder eingeschaltet oder die Szenen
  // geaendert wurden (Szene soll dann sofort neu gesendet werden)
  void onAutoResume(std::function<void()> callback) { autoResumeCallback_ = callback; }

  // Liefert ein JSON-Objekt mit den Live-Daten (DCS-BIOS, Sonnenstand, Szene)
  // fuer den Live-Tab. Wird bei jeder Anfrage an /api/live aufgerufen.
  void onLiveData(std::function<String()> callback) { liveCallback_ = callback; }

  // Standard-Missionsdatum: JSON lesen bzw. neues Datum pruefen und speichern
  // (Rueckgabe false mit Fehlertext in error)
  void onDateGet(std::function<String()> callback) { dateGetCallback_ = callback; }
  void onDateSet(std::function<bool(int, int, int, String &)> callback) { dateSetCallback_ = callback; }

private:
  void handleRoot();
  void handleState();
  void handleSet();
  void handleAuto();
  void handleSceneGet();
  void handleScenePost();
  void handleSceneReset();
  void handleLive();
  void handleDateGet();
  void handleDatePost();

  int argInt(const char *name, int defaultValue, int minValue, int maxValue);
  void sendState(int lamp);

  BRMesh &mesh_;
  SceneConfig &scene_;
  WebServer server_;
  uint16_t port_;
  String hostname_;
  bool started_ = false;
  bool manual_ = false;
  std::function<void()> autoResumeCallback_;
  std::function<String()> liveCallback_;
  std::function<String()> dateGetCallback_;
  std::function<bool(int, int, int, String &)> dateSetCallback_;
};
