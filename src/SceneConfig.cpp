#include "SceneConfig.h"

#include <Preferences.h>

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace {

const uint8_t SCENE_VERSION = 1;
const char *const NVS_NAMESPACE = "scene";
const char *const NVS_KEY = "data";

// ---- Interpolation ---------------------------------------------------------

template <typename K>
void findSegment(const K *keys, int count, float elevation,
                 const K *&a, const K *&c, float &t) {
  a = c = &keys[0];
  t = 0.0f;
  if (count < 2 || elevation <= keys[0].elevation) return;
  if (elevation >= keys[count - 1].elevation) {
    a = c = &keys[count - 1];
    return;
  }
  for (int i = 0; i < count - 1; i++) {
    if (elevation >= keys[i].elevation && elevation <= keys[i + 1].elevation) {
      a = &keys[i];
      c = &keys[i + 1];
      float span = c->elevation - a->elevation;
      t = span > 0.0f ? (elevation - a->elevation) / span : 0.0f;
      return;
    }
  }
}

uint8_t lerp8(uint8_t a, uint8_t b, float t) {
  float v = a + ((float)b - (float)a) * t;
  if (v < 0.0f) v = 0.0f;
  if (v > 255.0f) v = 255.0f;
  return (uint8_t)(v + 0.5f);
}

// ---- Sortieren und Pruefen -------------------------------------------------

template <typename K>
void sortByElevation(K *keys, int n) {
  for (int i = 1; i < n; i++) {
    K k = keys[i];
    int j = i - 1;
    while (j >= 0 && keys[j].elevation > k.elevation) {
      keys[j + 1] = keys[j];
      j--;
    }
    keys[j + 1] = k;
  }
}

template <typename K>
bool strictlyAscending(const K *keys, int n) {
  for (int i = 1; i < n; i++) {
    if (!(keys[i].elevation > keys[i - 1].elevation)) return false;
  }
  return true;
}

// ---- Text-Parser -----------------------------------------------------------

// Zerlegt s an sep; Rueckgabe Anzahl Teile, -1 bei mehr als maxOut Teilen
int splitString(const String &s, char sep, String *out, int maxOut) {
  if (s.length() == 0) return 0;
  int n = 0, start = 0;
  while (true) {
    int idx = s.indexOf(sep, start);
    String part = (idx < 0) ? s.substring(start) : s.substring(start, idx);
    part.trim();
    if (n >= maxOut) return -1;
    out[n++] = part;
    if (idx < 0) break;
    start = idx + 1;
  }
  return n;
}

bool parseFloat(const String &s, float &out) {
  if (s.length() == 0) return false;
  char *end = nullptr;
  double v = strtod(s.c_str(), &end);
  if (*end != '\0' || std::isnan(v) || std::isinf(v)) return false;
  out = (float)v;
  return true;
}

bool parseByte(const String &s, uint8_t &out) {
  if (s.length() == 0) return false;
  char *end = nullptr;
  long v = strtol(s.c_str(), &end, 10);
  if (*end != '\0' || v < 0 || v > 255) return false;
  out = (uint8_t)v;
  return true;
}

float roundTenth(float v) { return roundf(v * 10.0f) / 10.0f; }

// ---- Binaerformat im Flash -------------------------------------------------

uint16_t fletcher16(const uint8_t *d, size_t n) {
  uint16_t s1 = 0, s2 = 0;
  for (size_t i = 0; i < n; i++) {
    s1 = (s1 + d[i]) % 255;
    s2 = (s2 + s1) % 255;
  }
  return (uint16_t)((s2 << 8) | s1);
}

void put16(uint8_t *buf, size_t &p, int16_t v) {
  buf[p++] = (uint8_t)((uint16_t)v & 0xFF);
  buf[p++] = (uint8_t)(((uint16_t)v >> 8) & 0xFF);
}

int16_t get16(const uint8_t *buf, size_t &p) {
  uint16_t v = (uint16_t)buf[p] | ((uint16_t)buf[p + 1] << 8);
  p += 2;
  return (int16_t)v;
}

}  // namespace

// ---------------------------------------------------------------------------

void SceneConfig::resetDefaults() {
  switchElev_ = 12.0f;
  hyst_ = 0.5f;

  const RgbKey defaultRgb[] = {
      {-8.0f, 255, 105, 35, 12},   // Nacht
      {-3.0f, 255, 75, 20, 28},    // Daemmerung
      {4.0f, 255, 125, 45, 75},    // Sonnenauf-/untergang
      {12.0f, 255, 170, 90, 130},  // Uebergang zu Weiss
  };
  rgbCount_ = sizeof(defaultRgb) / sizeof(defaultRgb[0]);
  for (uint8_t i = 0; i < rgbCount_; i++) rgb_[i] = defaultRgb[i];

  const WhiteKey defaultWhite[] = {
      {12.0f, 130},  // Uebergang, gleiche Helligkeit wie Ende der Farbszene
      {20.0f, 170},
      {35.0f, 220},  // Tag
  };
  whiteCount_ = sizeof(defaultWhite) / sizeof(defaultWhite[0]);
  for (uint8_t i = 0; i < whiteCount_; i++) white_[i] = defaultWhite[i];

  resetState();
}

void SceneConfig::begin() {
  resetDefaults();
  if (load()) {
    Serial.println("Lichtszenen aus dem Flash geladen.");
  } else {
    Serial.println("Keine gueltigen Lichtszenen gespeichert, Standardwerte aktiv.");
  }
}

size_t SceneConfig::serialize(uint8_t *buf) const {
  size_t p = 0;
  buf[p++] = SCENE_VERSION;
  put16(buf, p, (int16_t)lroundf(switchElev_ * 10.0f));
  buf[p++] = (uint8_t)lroundf(hyst_ * 10.0f);

  buf[p++] = rgbCount_;
  for (uint8_t i = 0; i < rgbCount_; i++) {
    put16(buf, p, (int16_t)lroundf(rgb_[i].elevation * 10.0f));
    buf[p++] = rgb_[i].r;
    buf[p++] = rgb_[i].g;
    buf[p++] = rgb_[i].b;
    buf[p++] = rgb_[i].brightness;
  }

  buf[p++] = whiteCount_;
  for (uint8_t i = 0; i < whiteCount_; i++) {
    put16(buf, p, (int16_t)lroundf(white_[i].elevation * 10.0f));
    buf[p++] = white_[i].brightness;
  }

  uint16_t cs = fletcher16(buf, p);
  put16(buf, p, (int16_t)cs);
  return p;
}

bool SceneConfig::save() {
  uint8_t buf[128];
  size_t len = serialize(buf);

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, false)) {
    Serial.println("Lichtszenen: Flash-Speicher konnte nicht geoeffnet werden.");
    return false;
  }
  size_t written = prefs.putBytes(NVS_KEY, buf, len);
  prefs.end();

  if (written != len) {
    Serial.println("Lichtszenen: Speichern fehlgeschlagen.");
    return false;
  }
  return true;
}

bool SceneConfig::load() {
  uint8_t buf[128];

  Preferences prefs;
  if (!prefs.begin(NVS_NAMESPACE, true)) return false;  // Namespace existiert noch nicht
  size_t len = prefs.getBytesLength(NVS_KEY);
  if (len < 8 || len > sizeof(buf)) {
    prefs.end();
    return false;
  }
  prefs.getBytes(NVS_KEY, buf, len);
  prefs.end();

  // Pruefsumme
  size_t csPos = len - 2;
  size_t q = csPos;
  uint16_t stored = (uint16_t)get16(buf, q);
  if (fletcher16(buf, csPos) != stored) return false;

  size_t p = 0;
  if (buf[p++] != SCENE_VERSION) return false;

  float sw = get16(buf, p) / 10.0f;
  float hy = buf[p++] / 10.0f;

  uint8_t rc = buf[p++];
  if (rc < 2 || rc > MAX_KEYS || p + (size_t)rc * 6 > csPos) return false;
  RgbKey tmpRgb[MAX_KEYS];
  for (uint8_t i = 0; i < rc; i++) {
    tmpRgb[i].elevation = get16(buf, p) / 10.0f;
    tmpRgb[i].r = buf[p++];
    tmpRgb[i].g = buf[p++];
    tmpRgb[i].b = buf[p++];
    tmpRgb[i].brightness = buf[p++];
  }

  if (p >= csPos) return false;
  uint8_t wc = buf[p++];
  if (wc < 2 || wc > MAX_KEYS || p + (size_t)wc * 3 != csPos) return false;
  WhiteKey tmpWhite[MAX_KEYS];
  for (uint8_t i = 0; i < wc; i++) {
    tmpWhite[i].elevation = get16(buf, p) / 10.0f;
    tmpWhite[i].brightness = buf[p++];
  }

  if (!strictlyAscending(tmpRgb, rc) || !strictlyAscending(tmpWhite, wc)) return false;

  switchElev_ = sw;
  hyst_ = hy;
  rgbCount_ = rc;
  for (uint8_t i = 0; i < rc; i++) rgb_[i] = tmpRgb[i];
  whiteCount_ = wc;
  for (uint8_t i = 0; i < wc; i++) white_[i] = tmpWhite[i];
  resetState();
  return true;
}

// ---- Auswertung -------------------------------------------------------------

SceneOutput SceneConfig::evaluate(float elevationDeg) {
  // Modus mit Hysterese bestimmen
  if (!modeInit_) {
    whiteMode_ = elevationDeg >= switchElev_;
    modeInit_ = true;
  } else if (!whiteMode_ && elevationDeg >= switchElev_ + hyst_) {
    whiteMode_ = true;
  } else if (whiteMode_ && elevationDeg <= switchElev_ - hyst_) {
    whiteMode_ = false;
  }

  SceneOutput out;
  memset(&out, 0, sizeof(out));
  out.white = whiteMode_;

  if (whiteMode_) {
    const WhiteKey *a, *c;
    float t;
    findSegment(white_, whiteCount_, elevationDeg, a, c, t);
    out.warm = WHITE_VALUE;  // Lampe reagiert nur auf 255/255, RGB bleibt 0
    out.cold = WHITE_VALUE;
    out.brightness = lerp8(a->brightness, c->brightness, t);
  } else {
    const RgbKey *a, *c;
    float t;
    findSegment(rgb_, rgbCount_, elevationDeg, a, c, t);
    out.r = lerp8(a->r, c->r, t);
    out.g = lerp8(a->g, c->g, t);
    out.b = lerp8(a->b, c->b, t);
    out.brightness = lerp8(a->brightness, c->brightness, t);
  }
  return out;
}

// ---- Webinterface: JSON und Aktualisierung ------------------------------------

String SceneConfig::toJson() const {
  String j;
  j.reserve(900);
  j += "{\"max\":";
  j += (unsigned)MAX_KEYS;
  j += ",\"switch\":";
  j += String(switchElev_, 1);
  j += ",\"hyst\":";
  j += String(hyst_, 1);

  j += ",\"rgb\":[";
  for (uint8_t i = 0; i < rgbCount_; i++) {
    if (i) j += ',';
    j += "{\"e\":";
    j += String(rgb_[i].elevation, 1);
    j += ",\"r\":";
    j += (unsigned)rgb_[i].r;
    j += ",\"g\":";
    j += (unsigned)rgb_[i].g;
    j += ",\"b\":";
    j += (unsigned)rgb_[i].b;
    j += ",\"h\":";
    j += (unsigned)rgb_[i].brightness;
    j += '}';
  }

  j += "],\"white\":[";
  for (uint8_t i = 0; i < whiteCount_; i++) {
    if (i) j += ',';
    j += "{\"e\":";
    j += String(white_[i].elevation, 1);
    j += ",\"h\":";
    j += (unsigned)white_[i].brightness;
    j += '}';
  }
  j += "]}";
  return j;
}

bool SceneConfig::update(const String &switchElev, const String &hysteresis,
                         const String &rgbList, const String &whiteList,
                         String &error) {
  float sw, hy;
  if (!parseFloat(switchElev, sw) || sw < -20.0f || sw > 60.0f) {
    error = "Invalid switch elevation (-20 to 60 degrees)";
    return false;
  }
  if (!parseFloat(hysteresis, hy) || hy < 0.0f || hy > 5.0f) {
    error = "Invalid hysteresis (0 to 5 degrees)";
    return false;
  }

  // ---- Farbtabelle ----
  String rows[MAX_KEYS + 1];
  int rc = splitString(rgbList, ';', rows, MAX_KEYS + 1);
  if (rc < 2 || rc > MAX_KEYS) {
    error = "The colour table needs 2 to 10 rows";
    return false;
  }
  RgbKey tmpRgb[MAX_KEYS];
  for (int i = 0; i < rc; i++) {
    String f[5];
    if (splitString(rows[i], ',', f, 5) != 5) {
      error = "Colour row " + String(i + 1) + ": 5 values expected";
      return false;
    }
    float e;
    uint8_t r, g, b, h;
    if (!parseFloat(f[0], e) || e < -30.0f || e > 90.0f) {
      error = "Colour row " + String(i + 1) + ": invalid elevation (-30 to 90)";
      return false;
    }
    if (!parseByte(f[1], r) || !parseByte(f[2], g) || !parseByte(f[3], b) ||
        !parseByte(f[4], h)) {
      error = "Colour row " + String(i + 1) + ": R, G, B and brightness must be 0 to 255";
      return false;
    }
    tmpRgb[i] = {roundTenth(e), r, g, b, h};
  }
  sortByElevation(tmpRgb, rc);
  if (!strictlyAscending(tmpRgb, rc)) {
    error = "Colour table: elevations must be unique";
    return false;
  }

  // ---- Weisstabelle ----
  int wc = splitString(whiteList, ';', rows, MAX_KEYS + 1);
  if (wc < 2 || wc > MAX_KEYS) {
    error = "The white table needs 2 to 10 rows";
    return false;
  }
  WhiteKey tmpWhite[MAX_KEYS];
  for (int i = 0; i < wc; i++) {
    String f[2];
    if (splitString(rows[i], ',', f, 2) != 2) {
      error = "White row " + String(i + 1) + ": 2 values expected";
      return false;
    }
    float e;
    uint8_t h;
    if (!parseFloat(f[0], e) || e < -30.0f || e > 90.0f) {
      error = "White row " + String(i + 1) + ": invalid elevation (-30 to 90)";
      return false;
    }
    if (!parseByte(f[1], h)) {
      error = "White row " + String(i + 1) + ": brightness must be 0 to 255";
      return false;
    }
    tmpWhite[i] = {roundTenth(e), h};
  }
  sortByElevation(tmpWhite, wc);
  if (!strictlyAscending(tmpWhite, wc)) {
    error = "White table: elevations must be unique";
    return false;
  }

  // ---- Uebernehmen und speichern ----
  switchElev_ = roundTenth(sw);
  hyst_ = roundTenth(hy);
  rgbCount_ = (uint8_t)rc;
  for (int i = 0; i < rc; i++) rgb_[i] = tmpRgb[i];
  whiteCount_ = (uint8_t)wc;
  for (int i = 0; i < wc; i++) white_[i] = tmpWhite[i];
  resetState();

  if (!save()) {
    error = "Valid, but saving to flash failed";
    return false;
  }
  return true;
}
