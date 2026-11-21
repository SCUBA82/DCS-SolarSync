#include <Arduino.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <ArduinoOTA.h>
#include <Preferences.h>
#include "BRMesh.h"
#include "config.h"
#include "SceneConfig.h"
#include "WebControl.h"
#include <WiFi.h>


#ifdef DCSBIOS_WIFI_FORK
  #define DCSBIOS_DISABLE_SERVO
  #define DCSBIOS_LAN
  #define DCSBIOS_ESP32
#endif

#include <DcsBios.h>
#include "C:\Users\chris\Saved Games\DCS\Scripts\DCS-BIOS\doc\Addresses.h" //Custom Addresses for MissionDate and MapId may be defined here, if not already in DCS-BIOS library



BRMesh mesh(my_key, 2, 2);
SceneConfig scene;
WebControl web(mesh, scene);

int lat_deg = 0;
// ACHTUNG: In DCS-BIOS heissen LAT_SEC/LON_SEC zwar "Seconds", enthalten aber die ganzen
// BOGENMINUTEN (0..59). LAT_SEC_FRAC/LON_SEC_FRAC ist der Bruchteil dieser Minute (geteilt durch 65535).
// Beispiel 42 Grad 14' 38": DEG = 42, SEC = 14, SEC_FRAC = 0,6333 * 65535 (die 38 Bogensekunden).
int lat_sec = 0;
int lat_frac = 0;
char lat_dir[2] = "N";
int lon_deg = 0;
int lon_sec = 0;
int lon_frac = 0;
char lon_dir[2] = "E";

bool gotLatDeg = false;
bool gotLatSec = false;
bool gotLatDir = false;
bool gotLonDeg = false;
bool gotLonSec = false;
bool gotLonDir = false;

// Missionszeit: MOD_TIME als Text, Startzeit aus HIGH/LOW
char missionTime[7] = "0";
uint16_t timeStartHigh = 0;
uint16_t timeStartLow = 0;
bool gotTimeStartHigh = false;
bool gotTimeStartLow = false;

const unsigned long MOD_TIME_TIMEOUT_MS = 10000;
unsigned long lastModTimeChange = 0;
bool fallbackActive = false;
bool modTimeSeen = false;   // MOD_TIME wurde seit dem Start mindestens einmal empfangen
//bool lightInitialized = false;
//uint8_t lastR = 0, lastG = 0, lastB = 0, lastWarm = 0, lastCold = 0, lastBrightness = 0;

void copyDcsString(char *destination, size_t destinationSize, const char *source) {
  if (destinationSize == 0) return;
  if (source == nullptr) {
    destination[0] = '\0';
    return;
  }
  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

//OTA 
#ifdef DCSBIOS_WIFI_FORK
void setupOTA()
{
    // DCS-BIOS WiFi Fork hat die WLAN-Verbindung bereits aufgebaut.
    // Kurz warten, bis eine IP vorhanden ist.
    uint32_t start = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
        delay(10);
    }

    if (WiFi.status() == WL_CONNECTED) {

        ArduinoOTA.setHostname(OTA_HOSTNAME);

        // Optionales Passwort:
        // ArduinoOTA.setPassword("dein-passwort");

        ArduinoOTA.onStart([]() {
            Serial.println();
            Serial.println("OTA update started");

        });

        ArduinoOTA.onEnd([]() {
            Serial.println("\nOTA update finished");
        });

        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            Serial.printf(
                "OTA: %u%%\r",
                (progress * 100) / total
            );
        });

        ArduinoOTA.onError([](ota_error_t error) {
            Serial.printf(
                "\nOTA error [%u]: ",
                error
            );

            if (error == OTA_AUTH_ERROR)
                Serial.println("Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                Serial.println("Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                Serial.println("Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                Serial.println("Receive Failed");
            else if (error == OTA_END_ERROR)
                Serial.println("End Failed");
        });

        ArduinoOTA.begin();

        Serial.println();
        Serial.println("OTA ready");
        Serial.print("Hostname: ");
        Serial.println(OTA_HOSTNAME);
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }
    else {
        Serial.println("OTA: WiFi not connected");
    }
}
#endif


// DCS-BIOS: Missionsstartzeit HIGH/LOW
void onTimeStartHighChange(unsigned int newValue) {
  timeStartHigh = static_cast<uint16_t>(newValue);
  gotTimeStartHigh = true;
}

DcsBios::IntegerBuffer timeStartHighBuffer(
    CommonData_TIME_START_HIGH, onTimeStartHighChange);

void onTimeStartLowChange(unsigned int newValue) {
  timeStartLow = static_cast<uint16_t>(newValue);
  gotTimeStartLow = true;
}

DcsBios::IntegerBuffer timeStartLowBuffer(
    CommonData_TIME_START_LOW, onTimeStartLowChange);

void onLatDegChange(unsigned int newValue) {
  lat_deg = static_cast<int>(newValue);
  gotLatDeg = true;
}
DcsBios::IntegerBuffer latDegBuffer(CommonData_LAT_DEG, onLatDegChange);

void onLatSecChange(unsigned int newValue) {
  lat_sec = static_cast<int>(newValue);
  gotLatSec = true;
}
DcsBios::IntegerBuffer latSecBuffer(CommonData_LAT_SEC, onLatSecChange);

void onLatSecFracChange(unsigned int newValue) {
  lat_frac = static_cast<int>(newValue);
}
DcsBios::IntegerBuffer latSecFracBuffer(CommonData_LAT_SEC_FRAC, onLatSecFracChange);

void onLatZDirChange(char *newValue) {
  copyDcsString(lat_dir, sizeof(lat_dir), newValue);
  gotLatDir = true;
}
DcsBios::StringBuffer<1> latZDirBuffer(CommonData_LAT_Z_DIR_A, onLatZDirChange);

void onLonDegChange(unsigned int newValue) {
  lon_deg = static_cast<int>(newValue);
  gotLonDeg = true;
}
DcsBios::IntegerBuffer lonDegBuffer(CommonData_LON_DEG, onLonDegChange);

void onLonSecChange(unsigned int newValue) {
  lon_sec = static_cast<int>(newValue);
  gotLonSec = true;
}
DcsBios::IntegerBuffer lonSecBuffer(CommonData_LON_SEC, onLonSecChange);

void onLonSecFracChange(unsigned int newValue) {
  lon_frac = static_cast<int>(newValue);
}
DcsBios::IntegerBuffer lonSecFracBuffer(CommonData_LON_SEC_FRAC, onLonSecFracChange);

void onLonZDirChange(char *newValue) {
  copyDcsString(lon_dir, sizeof(lon_dir), newValue);
  gotLonDir = true;
}
DcsBios::StringBuffer<1> lonZDirBuffer(CommonData_LON_Z_DIR_A, onLonZDirChange);

void onModTimeChange(char *newValue) {
  copyDcsString(missionTime, sizeof(missionTime), newValue);
  lastModTimeChange = millis();
  modTimeSeen = true;
}
DcsBios::StringBuffer<6> modTimeBuffer(CommonData_MOD_TIME_A, onModTimeChange);

unsigned int altMslFt = 0;
bool gotAltMsl = false;

void onAltMslFtChange(unsigned int newValue) {
  altMslFt = newValue;
  gotAltMsl = true;
}
DcsBios::IntegerBuffer altMslFtBuffer(CommonData_ALT_MSL_FT, onAltMslFtChange);

void printDcsValues() {
  
  Serial.printf("Latitude:  %d deg %d.%05d min %s\n", lat_deg, lat_sec, (int)((long long)lat_frac * 100000LL / 65535), lat_dir);
  Serial.printf("Longitude: %d deg %d.%05d min %s\n", lon_deg, lon_sec, (int)((long long)lon_frac * 100000LL / 65535), lon_dir);
  Serial.printf("Mission start HIGH: %u\n", timeStartHigh);
  Serial.printf("Mission start LOW: %u\n", timeStartLow);
  Serial.printf("Mission elapsed seconds: %s\n", missionTime);
  Serial.printf("Altitude MSL (ft): %u\n", altMslFt);
}

// ---------------------------------------------------------------------------
// Automatische Lichtszene anhand des Sonnenstands
// ---------------------------------------------------------------------------

// Ersatz-Missionsdatum, solange von DCS-BIOS noch kein Datum empfangen wurde: 01.07.2026
const int MISSION_YEAR = 2026;
const int MISSION_MONTH = 7;
const int MISSION_DAY = 1;

// Missionsdatum aus DCS: Der Hook MissionDateHook.lua schreibt es in eine Datei, das DCS-BIOS-Modul
// CommonData_MissionDate (dcs/MissionDate.lua) exportiert es als CommonData_MISSION_YEAR / _MONTH / _DAY
// (siehe dcs/ und README). Die Adressen kommen aus dem DCS-BIOS Reference Tool.
// Stehen die Eintraege schon in einer Headerdatei der DCS-BIOS-Bibliothek, wird nichts weiter gebraucht.
// Sonst hier einkommentieren und im Format der vorhandenen Eintraege (z.B. CommonData_LAT_DEG) ausfuellen.
// Ohne diese Eintraege gilt das Ersatzdatum oben.
//#define CommonData_MISSION_YEAR  0x0486, 0xFFFF, 0   // Adresse, Maske, Shift: Platzhalter ersetzen, keine <> setzen
//#define CommonData_MISSION_MONTH 0x0464, 0x1E00, 9   // Adresse, Maske, Shift: Platzhalter ersetzen, keine <> setzen
//#define CommonData_MISSION_DAY   0488, 0x001F, 0   // Adresse, Maske, Shift: Platzhalter ersetzen, keine <> setzen

// Standarddatum: gilt, bis DCS ein Datum liefert. Im Webinterface einstellbar und im Flash gespeichert
// (die Konstanten oben sind nur der Werkswert).
int defaultYear = MISSION_YEAR;
int defaultMonth = MISSION_MONTH;
int defaultDay = MISSION_DAY;

// Aktuell verwendetes Missionsdatum (DCS-Datum oder Standarddatum)
int missionYear = MISSION_YEAR;
int missionMonth = MISSION_MONTH;
int missionDay = MISSION_DAY;
bool missionDateFromDcs = false;

// Zuletzt von DCS empfangene Werte (0 = nicht verfuegbar)
int dcsMissionYear = 0;
int dcsMissionMonth = 0;
int dcsMissionDay = 0;
bool gotDcsMissionYear = false;   // Rohwert wurde mindestens einmal empfangen (Diagnose, Live-Tab)
bool gotDcsMissionMonth = false;
bool gotDcsMissionDay = false;

// Uebernimmt das DCS-Datum, sobald alle drei Werte gueltig sind. Wird aus loop() aufgerufen
// (nicht aus den Callbacks), damit nie ein gemischtes Datum aus alten und neuen Werten benutzt wird.
// Meldet DCS wieder 0 (z.B. nach Missionsende), bleibt das zuletzt gueltige Datum erhalten.
void applyDcsMissionDate() {
  const bool valid = dcsMissionYear >= 1 && dcsMissionMonth >= 1 && dcsMissionMonth <= 12 &&
                     dcsMissionDay >= 1 && dcsMissionDay <= 31;
  if (valid) {
    if (!missionDateFromDcs || dcsMissionYear != missionYear ||
        dcsMissionMonth != missionMonth || dcsMissionDay != missionDay) {
      Serial.printf("Missionsdatum aus DCS: %04d-%02d-%02d\n",
                    dcsMissionYear, dcsMissionMonth, dcsMissionDay);
    }
    missionYear = dcsMissionYear;
    missionMonth = dcsMissionMonth;
    missionDay = dcsMissionDay;
    missionDateFromDcs = true;
    return;
  }
  // Noch kein DCS-Datum empfangen: Standarddatum verwenden (auch nach einer Aenderung im Webinterface)
  if (!missionDateFromDcs) {
    missionYear = defaultYear;
    missionMonth = defaultMonth;
    missionDay = defaultDay;
  }
}

#if defined(CommonData_MISSION_YEAR) && defined(CommonData_MISSION_MONTH) && defined(CommonData_MISSION_DAY)
const bool MISSION_DATE_CONFIGURED = true;
void onMissionYearChange(unsigned int newValue) {
  dcsMissionYear = (int)newValue;
  gotDcsMissionYear = true;
  Serial.printf("DCS MissionDate YEAR = %u\n", newValue);
}
void onMissionMonthChange(unsigned int newValue) {
  dcsMissionMonth = (int)newValue;
  gotDcsMissionMonth = true;
  Serial.printf("DCS MissionDate MONTH = %u\n", newValue);
}
void onMissionDayChange(unsigned int newValue) {
  dcsMissionDay = (int)newValue;
  gotDcsMissionDay = true;
  Serial.printf("DCS MissionDate DAY = %u\n", newValue);
}
DcsBios::IntegerBuffer missionYearBuffer(CommonData_MISSION_YEAR, onMissionYearChange);
DcsBios::IntegerBuffer missionMonthBuffer(CommonData_MISSION_MONTH, onMissionMonthChange);
DcsBios::IntegerBuffer missionDayBuffer(CommonData_MISSION_DAY, onMissionDayChange);
#else
// Es muessen ALLE DREI Eintraege definiert sein, sonst bleibt das DCS-Datum komplett abgeschaltet.
const bool MISSION_DATE_CONFIGURED = false;
#pragma message("MissionDate: CommonData_MISSION_YEAR, _MONTH und _DAY sind nicht alle definiert. Es gilt das Standarddatum.")
#endif

// ---------------------------------------------------------------------------
// DCS-Map-Zeitzonen
// ---------------------------------------------------------------------------
// Die Kartenkennung wird spaeter durch die eigene Kartenerkennung gesetzt.
// Dazu den Callback onMapIdChange() mit dem passenden DCS-BIOS-Buffer verbinden.
// Die IDs sind hier interne BRMesh-IDs und NICHT die internen DCS-Theatre-Namen.
enum DcsMapId : uint8_t {
  MAP_UNKNOWN = 0,
  MAP_CAUCASUS,
  MAP_NEVADA,
  MAP_NORMANDY,
  MAP_PERSIAN_GULF,
  MAP_CHANNEL,
  MAP_SYRIA,
  MAP_MARIANAS,
  MAP_FALKLANDS,
  MAP_SINAI,
  MAP_KOLA,
  MAP_AFGHANISTAN,
  MAP_IRAQ,
  MAP_GERMANY_CW
};

enum DstRule : uint8_t {
  DST_RULE_NONE = 0,
  DST_RULE_US,
  DST_RULE_EU
};

struct MapTimeZone {
  DcsMapId id;
  const char *name;
  const char *icao;
  const char *airport;
  int16_t stdOffsetMinutes;
  DstRule dstRule;
};

MapTimeZone MAP_TIME_ZONES[] = {
  { MAP_CAUCASUS,     "Caucasus",                    "UGKO", "Kutaisi Intl (Georgia)",             240, DST_RULE_NONE },
  { MAP_NEVADA,       "Nevada (NTTR)",               "KLAS", "Harry Reid Intl, Las Vegas",         -480, DST_RULE_US },
  { MAP_NORMANDY,     "Normandy",                    "LFRK", "Caen-Carpiquet",                      60, DST_RULE_EU },
  { MAP_PERSIAN_GULF, "Persian Gulf",                "OMDB", "Dubai Intl",                          240, DST_RULE_NONE },
  { MAP_CHANNEL,      "The Channel",                 "LFAC", "Calais-Dunkerque",                    60, DST_RULE_EU },
  { MAP_SYRIA,        "Syria",                       "OSDI", "Damascus Intl",                      180, DST_RULE_NONE },
  { MAP_MARIANAS,     "Marianas",                    "PGUM", "Antonio B. Won Pat Intl, Guam",      600, DST_RULE_NONE },
  { MAP_FALKLANDS,    "South Atlantic (Falklands)",  "EGYP", "RAF Mount Pleasant",                -240, DST_RULE_NONE },
  { MAP_SINAI,        "Sinai",                       "HESH", "Sharm El Sheikh Intl",                120, DST_RULE_NONE },
  { MAP_KOLA,         "Kola Peninsula",              "ULMM", "Murmansk",                            180, DST_RULE_NONE },
  { MAP_AFGHANISTAN,  "Afghanistan",                 "OAKB", "Kabul Intl",                          270, DST_RULE_NONE },
  { MAP_IRAQ,         "Iraq",                        "ORBI", "Baghdad Intl",                        180, DST_RULE_NONE },
  { MAP_GERMANY_CW,   "Germany Cold War",            "ETAR", "Ramstein AB",                         60, DST_RULE_EU }
};

const size_t MAP_TIME_ZONE_COUNT = sizeof(MAP_TIME_ZONES) / sizeof(MAP_TIME_ZONES[0]);


// Fallback-Zeitzone: Werkseinstellung UTC. Kann in der WebUI geaendert werden.
int16_t defaultUtcOffsetMinutes = 0;
DstRule defaultDstRule = DST_RULE_NONE;

const char *dstRuleName(DstRule rule) {
  switch (rule) {
    case DST_RULE_US: return "us";
    case DST_RULE_EU: return "eu";
    default: return "none";
  }
}

bool parseDstRule(int value, DstRule &rule) {
  if (value < 0 || value > 2) return false;
  rule = static_cast<DstRule>(value);
  return true;
}

void loadTimeZoneConfig() {
  Preferences prefs;
  if (!prefs.begin("timezone", true)) return;
  defaultUtcOffsetMinutes = (int16_t)prefs.getShort("fo", 0);
  defaultDstRule = static_cast<DstRule>(prefs.getUChar("fr", DST_RULE_NONE));
  if (defaultUtcOffsetMinutes < -720 || defaultUtcOffsetMinutes > 840 || defaultDstRule > DST_RULE_EU) {
    defaultUtcOffsetMinutes = 0;
    defaultDstRule = DST_RULE_NONE;
  }
  for (size_t i = 0; i < MAP_TIME_ZONE_COUNT; ++i) {
    char ok[8], rk[8];
    snprintf(ok, sizeof(ok), "o%u", (unsigned)MAP_TIME_ZONES[i].id);
    snprintf(rk, sizeof(rk), "r%u", (unsigned)MAP_TIME_ZONES[i].id);
    const int16_t off = (int16_t)prefs.getShort(ok, MAP_TIME_ZONES[i].stdOffsetMinutes);
    const uint8_t rule = prefs.getUChar(rk, MAP_TIME_ZONES[i].dstRule);
    if (off >= -720 && off <= 840 && rule <= DST_RULE_EU) {
      MAP_TIME_ZONES[i].stdOffsetMinutes = off;
      MAP_TIME_ZONES[i].dstRule = static_cast<DstRule>(rule);
    }
  }
  prefs.end();
}

bool setTimeZoneConfig(int id, int offsetMinutes, int ruleValue, String &error) {
  DstRule rule;
  if (offsetMinutes < -720 || offsetMinutes > 840) { error = "UTC offset must be between -12:00 and +14:00"; return false; }
  if (!parseDstRule(ruleValue, rule)) { error = "Invalid daylight-saving rule"; return false; }

  Preferences prefs;
  if (!prefs.begin("timezone", false)) { error = "Could not open flash storage"; return false; }
  if (id == 0) {
    prefs.putShort("fo", (int16_t)offsetMinutes);
    prefs.putUChar("fr", (uint8_t)rule);
    defaultUtcOffsetMinutes = (int16_t)offsetMinutes;
    defaultDstRule = rule;
  } else {
    bool found = false;
    for (size_t i = 0; i < MAP_TIME_ZONE_COUNT; ++i) {
      if ((int)MAP_TIME_ZONES[i].id != id) continue;
      char ok[8], rk[8];
      snprintf(ok, sizeof(ok), "o%u", (unsigned)id);
      snprintf(rk, sizeof(rk), "r%u", (unsigned)id);
      prefs.putShort(ok, (int16_t)offsetMinutes);
      prefs.putUChar(rk, (uint8_t)rule);
      MAP_TIME_ZONES[i].stdOffsetMinutes = (int16_t)offsetMinutes;
      MAP_TIME_ZONES[i].dstRule = rule;
      found = true;
      break;
    }
    if (!found) { prefs.end(); error = "Unknown map ID"; return false; }
  }
  prefs.end();
  return true;
}

String buildTimeZoneJson() {
  String j;
  j.reserve(1200);
  j += "{\"default\":{\"offset\":";
  j += String(defaultUtcOffsetMinutes / 60.0f, 2);
  j += ",\"rule\":"; j += (int)defaultDstRule;
  j += "},\"maps\":[";
  for (size_t i = 0; i < MAP_TIME_ZONE_COUNT; ++i) {
    if (i) j += ',';
    j += "{\"id\":"; j += (int)MAP_TIME_ZONES[i].id;
    j += ",\"name\":\""; j += MAP_TIME_ZONES[i].name;
    j += "\",\"offset\":"; j += String(MAP_TIME_ZONES[i].stdOffsetMinutes / 60.0f, 2);
    j += ",\"rule\":"; j += (int)MAP_TIME_ZONES[i].dstRule;
    j += '}';
  }
  j += "]}";
  return j;
}

volatile uint8_t currentDcsMapId = MAP_UNKNOWN;
bool gotDcsMapId = false;

// Platzhaltercallback: Die spaetere Kartenerkennung soll einen dieser internen
// MAP_* Werte liefern. Der Callback ist bewusst analog zu den uebrigen
// DCS-BIOS-Callbacks aufgebaut.
#ifdef CommonData_MAP_ID
  void onMapIdChange(unsigned int newValue) {
    if (newValue <= MAP_GERMANY_CW) {
      currentDcsMapId = static_cast<uint8_t>(newValue);
      gotDcsMapId = (newValue != MAP_UNKNOWN);
    } else {
      currentDcsMapId = MAP_UNKNOWN;
      gotDcsMapId = false;
    }
  }
  DcsBios::IntegerBuffer mapIdBuffer(CommonData_MAP_ID, onMapIdChange);
#endif



const MapTimeZone *findMapTimeZone() {
  if (!gotDcsMapId) return nullptr;
  const uint8_t mapId = currentDcsMapId;
  for (size_t i = 0; i < MAP_TIME_ZONE_COUNT; ++i) {
    if (MAP_TIME_ZONES[i].id == mapId) return &MAP_TIME_ZONES[i];
  }
  return nullptr;
}


bool isLeapYear(int year) {
  return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int daysInMonth(int year, int month) {
  const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  if (month == 2 && isLeapYear(year)) return 29;
  return days[month - 1];
}

// Wochentag: Sonntag = 0 ... Samstag = 6.
int dayOfWeek(int year, int month, int day) {
  static const int offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (month < 3) --year;
  return (year + year / 4 - year / 100 + year / 400 + offsets[month - 1] + day) % 7;
}

int nthSundayOfMonth(int year, int month, int nth) {
  const int firstSunday = 1 + (7 - dayOfWeek(year, month, 1)) % 7;
  return firstSunday + (nth - 1) * 7;
}

int lastSundayOfMonth(int year, int month) {
  const int lastDay = daysInMonth(year, month);
  return lastDay - dayOfWeek(year, month, lastDay);
}

bool isDaylightSavingTime(const MapTimeZone &tz, int year, int month, int day, int hour) {
  if (tz.dstRule == DST_RULE_NONE) return false;

  if (tz.dstRule == DST_RULE_US) {
    const int startDay = nthSundayOfMonth(year, 3, 2);
    const int endDay = nthSundayOfMonth(year, 11, 1);
    if (month < 3 || month > 11) return false;
    if (month > 3 && month < 11) return true;
    if (month == 3) return day > startDay || (day == startDay && hour >= 2);
    return day < endDay || (day == endDay && hour < 2);
  }

  // EU: Beginn am letzten Sonntag im Maerz um 02:00 Lokalzeit (CET),
  // Ende am letzten Sonntag im Oktober um 03:00 Lokalzeit (CEST).
  const int startDay = lastSundayOfMonth(year, 3);
  const int endDay = lastSundayOfMonth(year, 10);
  if (month < 3 || month > 10) return false;
  if (month > 3 && month < 10) return true;
  if (month == 3) return day > startDay || (day == startDay && hour >= 2);
  return day < endDay || (day == endDay && hour < 3);
}

bool getMapUtcOffset(const MapTimeZone &tz, int year, int month, int day,
                     int hour, float &utcOffsetHours, bool &dstActive) {
  dstActive = isDaylightSavingTime(tz, year, month, day, hour);
  utcOffsetHours = (tz.stdOffsetMinutes + (dstActive ? 60 : 0)) / 60.0f;
  return true;
}

enum LightScene : int {
  SCENE_NIGHT = 0,
  SCENE_TWILIGHT = 1,
  SCENE_SUNRISE_SUNSET = 2,
  SCENE_DAY_LOW = 3,
  SCENE_DAY = 4
};

int lastAutoScene = -1;
unsigned long lastSolarCalculation = 0;

bool parseMissionElapsedSeconds(const char *value, unsigned long &elapsedSeconds) {
  if (value == nullptr) return false;
  while (*value == ' ' || *value == '\t') ++value;
  if (*value == '\0') return false;

  char *endPtr = nullptr;
  unsigned long parsed = strtoul(value, &endPtr, 10);
  if (endPtr == value) return false;

  while (*endPtr == ' ' || *endPtr == '\t' ||
         *endPtr == '\r' || *endPtr == '\n') {
    ++endPtr;
  }

  if (*endPtr != '\0') return false;
  elapsedSeconds = parsed;
  return true;
}


void addDaysToDate(int &year, int &month, int &day, unsigned long daysToAdd) {
  while (daysToAdd-- > 0) {
    if (++day > daysInMonth(year, month)) {
      day = 1;
      if (++month > 12) {
        month = 1;
        ++year;
      }
    }
  }
}

// ---- Standard-Missionsdatum: Flash und Webinterface -------------------------

void loadDefaultMissionDate() {
  Preferences prefs;
  if (prefs.begin("mission", true)) {  // schlaegt fehl, solange der Namespace noch nicht existiert
    const int y = prefs.getUShort("y", 0);
    const int m = prefs.getUChar("m", 0);
    const int d = prefs.getUChar("d", 0);
    prefs.end();
    if (y >= 1900 && y <= 2200 && m >= 1 && m <= 12 && d >= 1 && d <= daysInMonth(y, m)) {
      defaultYear = y;
      defaultMonth = m;
      defaultDay = d;
      Serial.printf("Standard-Missionsdatum aus dem Flash: %04d-%02d-%02d\n", y, m, d);
    }
  }
  if (!missionDateFromDcs) {
    missionYear = defaultYear;
    missionMonth = defaultMonth;
    missionDay = defaultDay;
  }
}

// Prueft, uebernimmt und speichert das Standarddatum. Fehlertexte erscheinen im Webinterface.
bool setDefaultMissionDate(int y, int m, int d, String &error) {
  if (y < 1900 || y > 2200) { error = "Year must be 1900 to 2200"; return false; }
  if (m < 1 || m > 12) { error = "Month must be 1 to 12"; return false; }
  if (d < 1 || d > daysInMonth(y, m)) {
    error = "Day must be 1 to " + String(daysInMonth(y, m)) + " for this month";
    return false;
  }

  defaultYear = y;
  defaultMonth = m;
  defaultDay = d;

  Preferences prefs;
  if (!prefs.begin("mission", false)) {
    error = "Valid, but the flash storage could not be opened";
    return false;
  }
  prefs.putUShort("y", (uint16_t)y);
  prefs.putUChar("m", (uint8_t)m);
  prefs.putUChar("d", (uint8_t)d);
  prefs.end();

  Serial.printf("Standard-Missionsdatum gespeichert: %04d-%02d-%02d\n", y, m, d);
  return true;  // wirkt bei der naechsten Sonnenstandsberechnung (hoechstens ~1 s)
}

String buildDateJson() {
  char buf[160];
  snprintf(buf, sizeof(buf),
           "{\"y\":%d,\"m\":%d,\"d\":%d,\"ey\":%d,\"em\":%d,\"ed\":%d,\"src\":\"%s\"}",
           defaultYear, defaultMonth, defaultDay, missionYear, missionMonth, missionDay,
           missionDateFromDcs ? "dcs" : "default");
  return String(buf);
}

float normalizeDegrees(float value) {
  while (value < 0.0f) value += 360.0f;
  while (value >= 360.0f) value -= 360.0f;
  return value;
}

void calculateSolarPosition(
    int year, int month, int day, int hour, int minute, int second,
    double latitudeDeg, double longitudeDeg, float utcOffsetHours,
    float &elevationDeg, float &azimuthDeg) {
  const double pi = 3.14159265358979323846;

  const double localHours = hour + minute / 60.0 + second / 3600.0;
  double utcHours = localHours - utcOffsetHours;
  int utcYear = year;
  int utcMonth = month;
  int utcDay = day;

  // Julian Date muss UTC verwenden. Missionszeit und Datum bleiben im
  // DCS-Karten-Lokalzeitbezug; hier wird nur fuer die Astronomie umgerechnet.
  if (utcHours < 0.0) {
    utcHours += 24.0;
    if (--utcDay < 1) {
      if (--utcMonth < 1) { utcMonth = 12; --utcYear; }
      utcDay = daysInMonth(utcYear, utcMonth);
    }
  } else if (utcHours >= 24.0) {
    utcHours -= 24.0;
    if (++utcDay > daysInMonth(utcYear, utcMonth)) {
      utcDay = 1;
      if (++utcMonth > 12) { utcMonth = 1; ++utcYear; }
    }
  }

  int y = utcYear;
  int m = utcMonth;
  if (m <= 2) {
    y--;
    m += 12;
  }

  int A = y / 100;
  int B = 2 - A + A / 4;
  double jd = floor(365.25 * (y + 4716))
            + floor(30.6001 * (m + 1))
            + utcDay + B - 1524.5 + utcHours / 24.0;
  double T = (jd - 2451545.0) / 36525.0;

  double L0 = fmod(280.46646 + T * (36000.76983 + 0.0003032 * T), 360.0);
  if (L0 < 0) L0 += 360.0;

  double M = 357.52911 + T * (35999.05029 - 0.0001537 * T);
  double Mrad = M * pi / 180.0;
  double C = sin(Mrad) * (1.914602 - T * (0.004817 + 0.000014 * T))
           + sin(2.0 * Mrad) * (0.019993 - 0.000101 * T)
           + sin(3.0 * Mrad) * 0.000289;

  double trueLong = L0 + C;
  double omega = 125.04 - 1934.136 * T;
  double lambda = trueLong - 0.00569 - 0.00478 * sin(omega * pi / 180.0);

  double epsilon0 = 23.0 + (26.0 + (21.448 - T *
      (46.815 + T * (0.00059 - T * 0.001813))) / 60.0) / 60.0;
  double epsilon = epsilon0 + 0.00256 * cos(omega * pi / 180.0);
  double epsilonRad = epsilon * pi / 180.0;
  double lambdaRad = lambda * pi / 180.0;
  double declination = asin(sin(epsilonRad) * sin(lambdaRad));

  double yTerm = tan(epsilonRad / 2.0);
  yTerm *= yTerm;
  double L0rad = L0 * pi / 180.0;
  double eqTime = 4.0 * (180.0 / pi) *
      (yTerm * sin(2.0 * L0rad)
       - 2.0 * 0.016708634 * sin(Mrad)
       + 4.0 * 0.016708634 * yTerm * sin(Mrad) * cos(2.0 * L0rad)
       - 0.5 * yTerm * yTerm * sin(4.0 * L0rad)
       - 1.25 * 0.016708634 * 0.016708634 * sin(2.0 * Mrad));

  // Wahre Sonnenzeit aus UTC und geografischer Laenge.
  // Der Karten-UTC-Offset wurde bereits bei der Umrechnung local -> UTC angewandt.
  double trueSolarMinutes = fmod(
      utcHours * 60.0 + eqTime + 4.0 * longitudeDeg, 1440.0);
  if (trueSolarMinutes < 0.0) trueSolarMinutes += 1440.0;

  double hourAngleDeg = trueSolarMinutes / 4.0 - 180.0;
  double hourAngle = hourAngleDeg * pi / 180.0;
  double latitude = latitudeDeg * pi / 180.0;

  double cosZenith = sin(latitude) * sin(declination)
                   + cos(latitude) * cos(declination) * cos(hourAngle);
  if (cosZenith > 1.0) cosZenith = 1.0;
  if (cosZenith < -1.0) cosZenith = -1.0;

  double zenith = acos(cosZenith);
  double elevation = 90.0 - zenith * 180.0 / pi;

  double azimuth = atan2(sin(hourAngle),
      cos(hourAngle) * sin(latitude) - tan(declination) * cos(latitude));
  azimuth = normalizeDegrees((azimuth * 180.0 / pi) + 180.0);

  elevationDeg = (float)elevation;
  azimuthDeg = (float)azimuth;
}

const char *sceneName(int scene) {
  switch (scene) {
    case SCENE_NIGHT: return "Nacht";
    case SCENE_TWILIGHT: return "Daemmerung";
    case SCENE_SUNRISE_SUNSET: return "Sonnenaufgang/-untergang";
    case SCENE_DAY_LOW: return "Tief stehende Sonne";
    default: return "Tag";
  }
}

// ---------------------------------------------------------------------------
// Dynamischer Lichtverlauf anhand der Sonnenhoehe
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Dynamischer Lichtverlauf anhand der Sonnenhoehe (RGB + Weiss)
// ---------------------------------------------------------------------------




// Zustand
bool lightInitialized = false;
//bool whiteMode = false;
uint8_t lastR = 0, lastG = 0, lastB = 0, lastWarm = 0, lastCold = 0, lastBrightness = 0;

static uint8_t lerpByte(uint8_t a, uint8_t b, float t) {
  return (uint8_t)(a + (b - a) * t + 0.5f);
}

static uint8_t pctToWhite(uint8_t pct) {
  return (uint8_t)(((uint16_t)pct * SceneConfig::WHITE_VALUE + 50) / 100);
}

// Findet das Stuetzpunktpaar a/c und den Interpolationsfaktor t (0..1)
template <typename K>
static void findSegment(const K *keys, int count, float elevation, const K *&a, const K *&c, float &t) {
  a = c = &keys[0];
  t = 0.0f;
  if (elevation <= keys[0].elevation) return;
  if (elevation >= keys[count - 1].elevation) {
    a = c = &keys[count - 1];
    return;
  }
  for (int i = 0; i < count - 1; i++) {
    if (elevation >= keys[i].elevation && elevation <= keys[i + 1].elevation) {
      a = &keys[i];
      c = &keys[i + 1];
      t = (elevation - a->elevation) / (c->elevation - a->elevation);
      return;
    }
  }
}

const bool USE_ALTITUDE_HORIZON = true;
float currentHorizonDip = 0.0f;   // fuer die Log-Ausgabe

   void applyAutomaticLightScene(const SceneOutput &o, float sunElevationDeg, float sunAzimuthDeg,
                                 int hour, int minute, int second) {

     if (lightInitialized &&
         o.r == lastR && o.g == lastG && o.b == lastB &&
         o.warm == lastWarm && o.cold == lastCold &&
         o.brightness == lastBrightness) {
       return;
     }

     Serial.printf("Missionszeit: %02d:%02d:%02d, Hoehe %.1f (Kimmtiefe %.1f), Azimut %.1f -> %s RGB(%u,%u,%u) W(%u,%u) Helligkeit %u\n",
                   hour, minute, second, sunElevationDeg, currentHorizonDip , sunAzimuthDeg,
                   o.white ? "WEISS" : "RGB", o.r, o.g, o.b, o.warm, o.cold, o.brightness);

     if (!mesh.setAll(o.r, o.g, o.b, o.brightness, 0, o.warm, o.cold)) {
       Serial.println("Szene nicht gesendet (Queue voll?), neuer Versuch beim naechsten Durchlauf.");
       return;
     }

     lightInitialized = true;
     lastR = o.r; lastG = o.g; lastB = o.b;
     lastWarm = o.warm; lastCold = o.cold; lastBrightness = o.brightness;
   }


const unsigned long SOLAR_UPDATE_INTERVAL_MS = 750;

// ---------------------------------------------------------------------------
// Live-Daten fuer den Live-Tab im Webinterface
// ---------------------------------------------------------------------------

struct LiveSun {
  bool valid = false;        // true, wenn alle Werte vollstaendig berechnet wurden
  int year = 0, month = 0, day = 0, hour = 0, minute = 0, second = 0;
  float geometric = 0, dip = 0, effective = 0, azimuth = 0;
  SceneOutput out = {};
};
LiveSun live;

// Dezimalgrad aus den DCS-Koordinaten; false, wenn unvollstaendig oder ausserhalb des Bereichs
bool getDcsPosition(double &latitude, double &longitude) {
  if (!(gotLatDeg && gotLatSec && gotLatDir && gotLonDeg && gotLonSec && gotLonDir)) return false;
  // Grad + (ganze Minuten + Bruchteil einer Minute) / 60
  latitude = lat_deg + ((double)lat_sec + (double)lat_frac / 65535.0) / 60.0;
  longitude = lon_deg + ((double)lon_sec + (double)lon_frac / 65535.0) / 60.0;
  if (lat_dir[0] == 'S' || lat_dir[0] == 's') latitude = -latitude;
  if (lon_dir[0] == 'W' || lon_dir[0] == 'w') longitude = -longitude;
  return latitude >= -90.0 && latitude <= 90.0 && longitude >= -180.0 && longitude <= 180.0;
}

static void jsonNum(String &j, const char *key, bool valid, double v, int decimals) {
  j += ",\"";
  j += key;
  j += "\":";
  if (valid) j += String(v, decimals); else j += "null";
}

String buildLiveJson() {
  const unsigned long now = millis();
  // 0 = wartet auf erste Daten, 1 = Daten kommen, 2 = keine Aktualisierung (Pause oder getrennt)
  int dcs = 0;
  if (modTimeSeen) dcs = (now - lastModTimeChange > MOD_TIME_TIMEOUT_MS) ? 2 : 1;

  String j;
  j.reserve(840);
  j += "{\"dcs\":";
  j += dcs;
  j += ",\"age\":";
  j += modTimeSeen ? String((now - lastModTimeChange) / 1000UL) : String("null");
  j += ",\"fallback\":";
  j += fallbackActive ? "true" : "false";

  const MapTimeZone *mapTz = findMapTimeZone();
  if (mapTz) {
    j += ",\"map\":\"";
    j += mapTz->name;
    j += "\"";
  } else {
    j += ",\"map\":null";
  }
  j += ",\"tzsrc\":\"";
  j += mapTz ? "map" : "default";
  j += "\"";

  char dateBuf[64];
  snprintf(dateBuf, sizeof(dateBuf), ",\"mdate\":\"%04d-%02d-%02d\",\"msrc\":\"%s\"",
           missionYear, missionMonth, missionDay, missionDateFromDcs ? "dcs" : "default");
  j += dateBuf;
  j += ",\"dcfg\":";
  j += MISSION_DATE_CONFIGURED ? "true" : "false";
  jsonNum(j, "dy", gotDcsMissionYear, dcsMissionYear, 0);
  jsonNum(j, "dm", gotDcsMissionMonth, dcsMissionMonth, 0);
  jsonNum(j, "dd", gotDcsMissionDay, dcsMissionDay, 0);

  double lat = 0, lon = 0;
  const bool pos = getDcsPosition(lat, lon);
  jsonNum(j, "lat", pos, lat, 6);
  jsonNum(j, "lon", pos, lon, 6);
  jsonNum(j, "alt_ft", gotAltMsl, (double)altMslFt, 0);
  jsonNum(j, "alt_m", gotAltMsl, altMslFt * 0.3048, 0);

  j += ",\"sun\":";
  if (live.valid) {
    char buf[128];  // Das laengste Stueck (Szenenwerte) braucht rund 70 Zeichen
    snprintf(buf, sizeof(buf), "\"date\":\"%04d-%02d-%02d\",\"time\":\"%02d:%02d:%02d\"",
             live.year, live.month, live.day, live.hour, live.minute, live.second);
    j += "{";
    j += buf;
    jsonNum(j, "elev", true, live.geometric, 1);
    jsonNum(j, "dip", true, live.dip, 1);
    jsonNum(j, "eff", true, live.effective, 1);
    jsonNum(j, "az", true, live.azimuth, 1);
    snprintf(buf, sizeof(buf), ",\"white\":%s,\"r\":%u,\"g\":%u,\"b\":%u,\"br\":%u,\"w6\":%u,\"w7\":%u",
             live.out.white ? "true" : "false", live.out.r, live.out.g, live.out.b,
             live.out.brightness, live.out.warm, live.out.cold);
    j += buf;
    j += "}";
  } else {
    j += "null";
  }
  j += "}";
  return j;
}




// Um wie viele Grad der sichtbare Horizont in dieser Hoehe unter der Waagerechten liegt
float horizonDipDegrees(float heightMeters) {
  if (heightMeters <= 0.0f) return 0.0f;
  const double earthRadius = 6371000.0;
  double dip = acos(earthRadius / (earthRadius + heightMeters)) * 180.0 / 3.14159265358979323846;
  return (float)dip;
}


// apply = false: nur berechnen (fuer den Live-Tab), nichts an die Lampen senden
void updateAutomaticLighting(bool apply) {
  const unsigned long now = millis();
  if (now - lastSolarCalculation < SOLAR_UPDATE_INTERVAL_MS) return;
  lastSolarCalculation = now;
  live.valid = false;
 
   static unsigned long lastWaitMsg = 0;
  auto waitMsg = [&](const char *text) {
  if (now - lastWaitMsg >= 5000) {
     lastWaitMsg = now; Serial.println(text); 
  }
};
 

  if (!(gotLatDeg && gotLatSec && gotLatDir &&
        gotLonDeg && gotLonSec && gotLonDir)) {
    waitMsg("Warte auf vollstaendige DCS-Koordinaten.");
    return;
  }

  if (!(gotTimeStartHigh && gotTimeStartLow)) {
    waitMsg("Warte auf TIME_START_HIGH und TIME_START_LOW.");
    return;
  }

  unsigned long elapsedSeconds;
  if (!parseMissionElapsedSeconds(missionTime, elapsedSeconds)) {
    waitMsg("Warte auf gueltige Missionszeit in Sekunden; empfangen: '%s'");
    return;
  }

  // HIGH und LOW bilden zusammen einen 32-Bit-Sekundenwert.
  const uint32_t startSeconds =
      (static_cast<uint32_t>(timeStartHigh) << 16) |
      static_cast<uint32_t>(timeStartLow);

  // Startwert als Sekunden seit Mitternacht interpretieren.
  // Falls DCS-BIOS hier einen anderen Zeitbezug liefert, muss diese
  // Dekodierung an die Definition von TIME_START angepasst werden.
  const uint32_t startSecondsOfDay = startSeconds % 86400UL;

  const uint64_t totalSeconds =
      static_cast<uint64_t>(startSecondsOfDay) + elapsedSeconds;

  const unsigned long dayOffset = totalSeconds / 86400ULL;
  const unsigned long secondsOfDay = totalSeconds % 86400ULL;

  const int hour = secondsOfDay / 3600UL;
  const int minute = (secondsOfDay % 3600UL) / 60UL;
  const int second = secondsOfDay % 60UL;

  applyDcsMissionDate();
  int solarYear = missionYear;
  int solarMonth = missionMonth;
  int solarDay = missionDay;
  addDaysToDate(solarYear, solarMonth, solarDay, dayOffset);

  const MapTimeZone *mapTz = findMapTimeZone();
  float utcOffsetHours = 0.0f;
  bool dstActive = false;
  if (mapTz) {
    getMapUtcOffset(*mapTz, solarYear, solarMonth, solarDay, hour,
                    utcOffsetHours, dstActive);
  } else {
    MapTimeZone fallbackTz = { MAP_UNKNOWN, "Default", "", "", defaultUtcOffsetMinutes, defaultDstRule };
    getMapUtcOffset(fallbackTz, solarYear, solarMonth, solarDay, hour,
                    utcOffsetHours, dstActive);
  }

  double latitude = 0.0, longitude = 0.0;
  if (!getDcsPosition(latitude, longitude)) {
    Serial.println("Ungueltige DCS-Koordinaten; Sonnenstand wird nicht berechnet.");
    return;
  }

  float sunElevationDeg = 0.0f;
  float sunAzimuthDeg = 0.0f;

  calculateSolarPosition(
      solarYear, solarMonth, solarDay,
      hour, minute, second, latitude, longitude, utcOffsetHours,
      sunElevationDeg, sunAzimuthDeg);

    currentHorizonDip = 0.0f;
    if (USE_ALTITUDE_HORIZON && gotAltMsl) {
      currentHorizonDip = horizonDipDegrees(altMslFt * 0.3048f);
    }
    const float effectiveElevation = sunElevationDeg + currentHorizonDip;

    const SceneOutput o = scene.evaluate(effectiveElevation);

    live.year = solarYear; live.month = solarMonth; live.day = solarDay;
    live.hour = hour; live.minute = minute; live.second = second;
    live.geometric = sunElevationDeg; live.dip = currentHorizonDip;
    live.effective = effectiveElevation; live.azimuth = sunAzimuthDeg;
    live.out = o;
    live.valid = true;

    if (apply) applyAutomaticLightScene(o, effectiveElevation, sunAzimuthDeg, hour, minute, second);
    
}

// ---------------------------------------------------------------------------

void setup() {
  Serial.begin(115200);  // Nur Diagnoseausgabe; keine serielle Steuerung mehr.
  delay(500);
  mesh.begin("ESP32 as iBeacon");
  
  Serial.println("BRMesh-Controller bereit.");
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
  web.setHostname(OTA_HOSTNAME);   // für mDNS
  DcsBios::setup(ssid, passwd);
  loadDefaultMissionDate();
  loadTimeZoneConfig();
  scene.begin();
  web.begin();
  web.onLiveData(buildLiveJson);
  web.onDateGet(buildDateJson);
  web.onDateSet(setDefaultMissionDate);
  web.onTimeZoneGet(buildTimeZoneJson);
  web.onTimeZoneSet(setTimeZoneConfig);
  web.onAutoResume([]() { lightInitialized = false; scene.resetState(); });
  setupOTA();
   
}

void loop() {

#ifdef DCSBIOS_WIFI_FORK
   static uint32_t lastOTA = 0;
    if (millis() - lastOTA >= 20) {
        lastOTA = millis();
        ArduinoOTA.handle();
    }
#endif



  DcsBios::loop();
  mesh.loop();
  web.loop();

  // Im Handbetrieb wird weiter gerechnet (Live-Tab), aber nichts an die Lampen gesendet
  const bool manual = web.isManual();
  const bool timedOut = (millis() - lastModTimeChange) > MOD_TIME_TIMEOUT_MS;

  if (timedOut) {
    if (!manual && !fallbackActive) {
      fallbackActive = true;
      Serial.println("MOD_TIME seit 10 s unveraendert -> Standardszene.");
      mesh.setAll(0, 0, 0, 180, 0, SceneConfig::WHITE_VALUE, SceneConfig::WHITE_VALUE);
      lightInitialized = false;
    }
  } else {
    if (!manual && fallbackActive) {
      fallbackActive = false;
      Serial.println("MOD_TIME aktualisiert sich wieder.");
    }
    updateAutomaticLighting(!manual);
  }
}