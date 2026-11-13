#pragma once

#include <Arduino.h>
#include <BLEDevice.h>
#include <queue>
#include <vector>

/**
 * Steuerung von BRMesh/FastCon-BLE-Lampen ueber BLE-Advertising (ESP32).
 *
 * Senden nach dem Muster von esphome-fastcon: Befehle landen in einer
 * Warteschlange, mehrere Lampen/Befehle werden nicht blockierend nacheinander
 * ausgesendet. Dafuer muss mesh.loop() regelmaessig in loop() aufgerufen werden.
 *
 * Lampennummern sind 1-basiert (1 .. lampCount()).
 */
class BRMesh {
public:
  static constexpr uint8_t MAX_LAMPS = 4;

  // key:       4 Byte Netzwerkschluessel
  // lampCount: Anzahl angesteuerter Lampen (1..MAX_LAMPS)
  // repeats:   wie oft jeder Befehl ausgesendet wird (1 = wie esphome-fastcon)
  explicit BRMesh(const uint8_t key[4], uint8_t lampCount = 2,
                  uint8_t repeats = 1);

  // Initialisiert BLE (Advertising wird erst bei Bedarf gestartet)
  void begin(const char *deviceName = "ESP32 as iBeacon");

  // Muss in loop() aufgerufen werden: arbeitet die Sendewarteschlange ab
  void loop();

  // Advertising-Parameter (Standard wie esphome-fastcon)
  void setAdvInterval(uint16_t minUnits, uint16_t maxUnits);  // 0.625 ms Einheiten
  void setAdvDuration(uint16_t ms) { advDurationMs_ = ms; }
  void setAdvGap(uint16_t ms) { advGapMs_ = ms; }
  void setMaxQueueSize(size_t size) { maxQueueSize_ = size; }

  bool isQueueEmpty() const { return queue_.empty(); }
  size_t queueSize() const { return queue_.size(); }
  void clearQueue();

  // Ausgabe von Debug-Informationen auf Serial
  void setVerbose(bool verbose) { verbose_ = verbose; }

  uint8_t lampCount() const { return lampCount_; }
  bool isValidLamp(int lamp) const { return lamp >= 1 && lamp <= lampCount_; }

  // Alle Methoden stellen den Befehl nur in die Warteschlange.
  // Rueckgabe false bei ungueltiger Lampennummer, voller Queue oder Fehler.
  bool setPower(int lamp, bool on);
  bool setColor(int lamp, uint8_t r, uint8_t g, uint8_t b);
  bool setBrightness(int lamp, uint8_t brightness);
  bool setColorAndBrightness(int lamp, uint8_t r, uint8_t g, uint8_t b,
                             uint8_t brightness);

  // Weisskanaele: Byte 6 = Warm, Byte 7 = Kalt (laut esphome-fastcon, ungeprueft)
  bool setWhite(int lamp, uint8_t w6, uint8_t w7);

  // Setzt Farbe, Helligkeit und Weisskanaele einer Lampe und sendet nur ein Paket
  bool setLamp(int lamp, uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
               uint8_t w6, uint8_t w7);

  // Stellt den gespeicherten Zustand (Farbe + Weiss + Helligkeit) einer Lampe in die Queue
  bool sendColorAndBrightness(int lamp);

  // Setzt alle Lampen auf denselben Wert.
  // delayBetweenMs wird nicht mehr benoetigt und ignoriert (nur Kompatibilitaet).
  // Rueckgabe true, wenn alle Lampen in die Queue gestellt werden konnten.
  bool setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
              uint16_t delayBetweenMs = 0, uint8_t w6 = 0, uint8_t w7 = 0);

  // Gespeicherte Werte (0 bei ungueltiger Lampe)
  uint8_t red(int lamp) const;
  uint8_t green(int lamp) const;
  uint8_t blue(int lamp) const;
  uint8_t brightness(int lamp) const;
  uint8_t white6(int lamp) const;
  uint8_t white7(int lamp) const;

private:
  enum class AdvState : uint8_t { IDLE, ADVERTISING, GAP };

  static uint8_t brightnessToProtocol(uint8_t brightness);

  bool enqueueControl(const uint8_t *data, uint8_t length);
  bool startAdvertising(const std::vector<uint8_t> &command);

  uint8_t doGenerateCommand(int i, const uint8_t *data, uint8_t length,
                            int forward, int i2, uint8_t *&rfPayload);
  uint8_t getPayloadWithInnerRetry(int i, const uint8_t *data, int length,
                                   int i2, int forward, uint8_t *&payload);
  uint8_t packageBleFastconBody(int i, int i2, uint8_t sequence,
                                uint8_t safeKey, int forward,
                                const uint8_t *data, int length,
                                uint8_t *&payload);

  uint8_t key_[4];
  uint8_t lampCount_;
  uint8_t repeats_;
  bool verbose_ = false;
  bool begun_ = false;

  uint8_t sendSeq_ = 0;
  uint8_t sendCount_ = 1;

  // Sendewarteschlange und Advertising-Zustandsautomat
  std::queue<std::vector<uint8_t>> queue_;
  size_t maxQueueSize_ = 100;
  AdvState advState_ = AdvState::IDLE;
  uint32_t stateStart_ = 0;
  uint16_t advIntervalMin_ = 0x20;
  uint16_t advIntervalMax_ = 0x40;
  uint16_t advDurationMs_ = 50;
  uint16_t advGapMs_ = 10;

  uint8_t red_[MAX_LAMPS];
  uint8_t green_[MAX_LAMPS];
  uint8_t blue_[MAX_LAMPS];
  uint8_t brightness_[MAX_LAMPS];
  uint8_t white6_[MAX_LAMPS];
  uint8_t white7_[MAX_LAMPS];
};
