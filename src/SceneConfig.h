#pragma once

#include <Arduino.h>

// Stuetzpunkt der Farbszene: Sonnenhoehe -> RGB und Helligkeit
struct RgbKey {
  float elevation;
  uint8_t r, g, b, brightness;
};

// Stuetzpunkt der Weissszene: Sonnenhoehe -> Helligkeit
// (Warm- und Kaltweiss stehen fest auf SceneConfig::WHITE_VALUE)
struct WhiteKey {
  float elevation;
  uint8_t brightness;
};

// Ergebnis der Szenenauswertung
struct SceneOutput {
  bool white;  // true: Weissmodus, RGB = 0
  uint8_t r, g, b;
  uint8_t warm, cold;
  uint8_t brightness;
};

/**
 * Lichtszenen anhand der Sonnenhoehe.
 *
 * Zwei Tabellen (Farbe und Weiss) werden zwischen den Stuetzpunkten linear
 * interpoliert. Bei der Umschalthoehe wechselt die Ausgabe zwischen Farbe und
 * Weiss (mit Hysterese). Die Konfiguration wird im Flash (NVS / Preferences)
 * gespeichert und beim Start geladen.
 */
class SceneConfig {
public:
  static constexpr uint8_t MAX_KEYS = 10;
  static constexpr uint8_t WHITE_VALUE = 255;

  // Laedt die gespeicherte Konfiguration, sonst gelten die Standardwerte
  void begin();

  // Speichert die aktuelle Konfiguration im Flash
  bool save();

  // Standardwerte setzen (speichert nicht)
  void resetDefaults();

  // Modus (Farbe/Weiss) bei der naechsten Auswertung neu bestimmen
  void resetState() { modeInit_ = false; }

  // Berechnet Farbe/Weiss und Helligkeit fuer eine Sonnenhoehe
  SceneOutput evaluate(float elevationDeg);

  // Konfiguration als JSON (fuer das Webinterface)
  String toJson() const;

  // Prueft und uebernimmt eine neue Konfiguration und speichert sie.
  // Listenformat: Farbe "hoehe,r,g,b,hell;hoehe,r,g,b,hell;..."
  //               Weiss "hoehe,hell;hoehe,hell;..."
  // Rueckgabe false mit Fehlertext in error, wenn etwas ungueltig ist.
  bool update(const String &switchElev, const String &hysteresis,
              const String &rgbList, const String &whiteList, String &error);

private:
  size_t serialize(uint8_t *buf) const;
  bool load();

  float switchElev_ = 12.0f;
  float hyst_ = 0.5f;

  uint8_t rgbCount_ = 0;
  uint8_t whiteCount_ = 0;
  RgbKey rgb_[MAX_KEYS];
  WhiteKey white_[MAX_KEYS];

  bool whiteMode_ = false;
  bool modeInit_ = false;
};
