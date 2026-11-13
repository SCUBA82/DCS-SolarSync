#include "BRMesh.h"

#include <esp_gap_ble_api.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const uint8_t DEFAULT_KEY[] = {0x5e, 0x36, 0x7b, 0xc4};
const uint8_t DEFAULT_BLE_FASTCON_ADDRESS[] = {0xC1, 0xC2, 0xC3};
const uint8_t ADDR_LENGTH = 3;

uint8_t reverse8(uint8_t d) {
  uint8_t result = 0;
  for (uint8_t k = 0; k < 8; k++) result |= ((d >> k) & 1) << (7 - k);
  return result;
}

uint16_t reverse16(uint16_t d) {
  uint16_t result = 0;
  for (uint8_t k = 0; k < 16; k++) result |= ((d >> k) & 1) << (15 - k);
  return result;
}

void dump(const uint8_t *data, int length) {
  for (int i = 0; i < length; i++) printf("%2.2X", data[i]);
}

void whiteningInit(uint8_t val, uint8_t *ctx) {
  ctx[0] = 1;
  ctx[1] = (val >> 5) & 1;
  ctx[2] = (val >> 4) & 1;
  ctx[3] = (val >> 3) & 1;
  ctx[4] = (val >> 2) & 1;
  ctx[5] = (val >> 1) & 1;
  ctx[6] = val & 1;
}

void whiteningEncode(const uint8_t *data, int len, uint8_t *ctx,
                     uint8_t *result) {
  memcpy(result, data, len);

  for (int i = 0; i < len; i++) {
    int varC = ctx[3];
    int var14 = ctx[5];
    int var18 = ctx[6];
    int var10 = ctx[4];
    int var8 = var14 ^ ctx[2];
    int var4 = var10 ^ ctx[1];
    int _var = var18 ^ varC;
    int var0 = _var ^ ctx[0];
    int c = result[i];

    result[i] =
        ((c & 0x80) ^ ((var8 ^ var18) << 7)) +
        ((c & 0x40) ^ (var0 << 6)) +
        ((c & 0x20) ^ (var4 << 5)) +
        ((c & 0x10) ^ (var8 << 4)) +
        ((c & 0x08) ^ (_var << 3)) +
        ((c & 0x04) ^ (var10 << 2)) +
        ((c & 0x02) ^ (var14 << 1)) +
        ((c & 0x01) ^ (var18 << 0));

    ctx[2] = var4;
    ctx[3] = var8;
    ctx[4] = var8 ^ varC;
    ctx[5] = var0 ^ var10;
    ctx[6] = var4 ^ var14;
    ctx[0] = var8 ^ var18;
    ctx[1] = var0;
  }
}

uint16_t crc16(const uint8_t *addr, const uint8_t *data, uint8_t dataLength) {
  uint16_t crc = 0xffff;

  for (int8_t i = ADDR_LENGTH - 1; i >= 0; i--) {
    crc ^= addr[i] << 8;
    for (uint8_t ii = 0; ii < 4; ii++) {
      uint16_t tmp = crc << 1;
      if ((crc & 0x8000) != 0) tmp ^= 0x1021;
      crc = tmp << 1;
      if ((tmp & 0x8000) != 0) crc ^= 0x1021;
    }
  }

  for (uint8_t i = 0; i < dataLength; i++) {
    crc ^= reverse8(data[i]) << 8;
    for (uint8_t ii = 0; ii < 4; ii++) {
      uint16_t tmp = crc << 1;
      if ((crc & 0x8000) != 0) tmp ^= 0x1021;
      crc = tmp << 1;
      if ((tmp & 0x8000) != 0) crc ^= 0x1021;
    }
  }

  return ~reverse16(crc) & 0xffff;
}

uint8_t getRfPayload(const uint8_t *addr, const uint8_t *data,
                     uint8_t dataLength, uint8_t *&rfPayload) {
  const uint8_t dataOffset = 0x12;
  const uint8_t inverseOffset = 0x0f;
  uint8_t resultDataSize = dataOffset + ADDR_LENGTH + dataLength + 2;
  uint8_t *resultbuf = (uint8_t *)malloc(resultDataSize);
  if (!resultbuf) {
    rfPayload = nullptr;
    return 0;
  }

  memset(resultbuf, 0, resultDataSize);
  resultbuf[0x0f] = 0x71;
  resultbuf[0x10] = 0x0f;
  resultbuf[0x11] = 0x55;

  for (uint8_t j = 0; j < ADDR_LENGTH; j++)
    resultbuf[dataOffset + ADDR_LENGTH - j - 1] = addr[j];

  for (int j = 0; j < dataLength; j++)
    resultbuf[dataOffset + ADDR_LENGTH + j] = data[j];

  for (int i = inverseOffset; i < inverseOffset + ADDR_LENGTH + 3; i++)
    resultbuf[i] = reverse8(resultbuf[i]);

  int crc = crc16(addr, data, dataLength);
  resultbuf[resultDataSize - 2] = crc & 0xff;
  resultbuf[resultDataSize - 1] = (crc >> 8) & 0xff;

  rfPayload = resultbuf;
  return resultDataSize;
}

}  // namespace

// ---------------------------------------------------------------------------

BRMesh::BRMesh(const uint8_t key[4], uint8_t lampCount, uint8_t repeats)
    : lampCount_(lampCount > MAX_LAMPS ? MAX_LAMPS : (lampCount < 1 ? 1 : lampCount)),
      repeats_(repeats < 1 ? 1 : repeats) {
  memcpy(key_, key, 4);
  for (uint8_t i = 0; i < MAX_LAMPS; i++) {
    red_[i] = green_[i] = blue_[i] = brightness_[i] = 255;
    white6_[i] = white7_[i] = 0;
  }
}

void BRMesh::begin(const char *deviceName) {
  BLEDevice::init(deviceName);
  begun_ = true;
}

void BRMesh::setAdvInterval(uint16_t minUnits, uint16_t maxUnits) {
  advIntervalMin_ = minUnits;
  advIntervalMax_ = maxUnits < minUnits ? minUnits : maxUnits;
}

void BRMesh::clearQueue() {
  std::queue<std::vector<uint8_t>> empty;
  std::swap(queue_, empty);
}

uint8_t BRMesh::brightnessToProtocol(uint8_t brightness) {
  return (uint16_t(brightness) * 127 + 127) / 255;
}

// --- Sendewarteschlange / Advertising --------------------------------------

void BRMesh::loop() {
  if (!begun_) return;
  const uint32_t now = millis();

  switch (advState_) {
    case AdvState::IDLE: {
      if (queue_.empty()) return;

      std::vector<uint8_t> command = std::move(queue_.front());
      queue_.pop();

      if (!startAdvertising(command)) return;  // Befehl verworfen

      advState_ = AdvState::ADVERTISING;
      stateStart_ = now;
      break;
    }

    case AdvState::ADVERTISING:
      if (now - stateStart_ >= advDurationMs_) {
        esp_ble_gap_stop_advertising();
        advState_ = AdvState::GAP;
        stateStart_ = now;
      }
      break;

    case AdvState::GAP:
      if (now - stateStart_ >= advGapMs_) {
        advState_ = AdvState::IDLE;
      }
      break;
  }
}

bool BRMesh::startAdvertising(const std::vector<uint8_t> &command) {
  // Flags (3) + Laengenbyte/Typ/Hersteller-ID (4) + Payload muessen in 31 Bytes passen
  if (command.empty() || command.size() > 31 - 7) {
    Serial.println("Fehler: BRMesh-Paket hat ungueltige Laenge.");
    return false;
  }

  uint8_t raw[31] = {0};
  uint8_t len = 0;

  // Flags
  raw[len++] = 2;
  raw[len++] = ESP_BLE_AD_TYPE_FLAG;
  raw[len++] = ESP_BLE_ADV_FLAG_BREDR_NOT_SPT | ESP_BLE_ADV_FLAG_GEN_DISC;

  // Hersteller-Daten: Laenge = Typ (1) + ID (2) + Payload
  raw[len++] = command.size() + 3;
  raw[len++] = ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE;
  raw[len++] = 0xF0;  // Hersteller-ID 0xFFF0, little endian
  raw[len++] = 0xFF;

  memcpy(&raw[len], command.data(), command.size());
  len += command.size();

  esp_ble_adv_params_t params;
  memset(&params, 0, sizeof(params));
  params.adv_int_min = advIntervalMin_;
  params.adv_int_max = advIntervalMax_;
  params.adv_type = ADV_TYPE_NONCONN_IND;
  params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
  params.channel_map = ADV_CHNL_ALL;
  params.adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY;

  esp_err_t err = esp_ble_gap_config_adv_data_raw(raw, len);
  if (err != ESP_OK) {
    Serial.printf("BRMesh: Fehler beim Setzen der Advertising-Daten: %s\n",
                  esp_err_to_name(err));
    return false;
  }

  err = esp_ble_gap_start_advertising(&params);
  if (err != ESP_OK) {
    Serial.printf("BRMesh: Fehler beim Start des Advertisings: %s\n",
                  esp_err_to_name(err));
    return false;
  }
  return true;
}

bool BRMesh::enqueueControl(const uint8_t *data, uint8_t length) {
  if (!begun_) {
    Serial.println("Fehler: BRMesh::begin() wurde nicht aufgerufen.");
    return false;
  }

  uint8_t *rfPayload = nullptr;
  uint8_t rfPayloadLength = doGenerateCommand(5, data, length, true, 0, rfPayload);
  if (!rfPayloadLength || !rfPayload) return false;

  std::vector<uint8_t> command(rfPayload, rfPayload + rfPayloadLength);
  free(rfPayload);

  // Jeder Befehl wird repeats_ mal mit identischem Paket ausgesendet
  if (queue_.size() + repeats_ > maxQueueSize_) {
    Serial.printf("BRMesh: Queue voll (%u), Befehl verworfen.\n",
                  (unsigned)queue_.size());
    return false;
  }
  for (uint8_t i = 0; i < repeats_; i++) queue_.push(command);
  return true;
}

// --- Oeffentliche API ------------------------------------------------------

bool BRMesh::setPower(int lamp, bool on) {
  if (!isValidLamp(lamp)) {
    Serial.printf("Fehler: Lampennummer muss zwischen 1 und %u liegen.\n",
                  lampCount_);
    return false;
  }

  uint8_t data[] = {0x22, 0x00, 0xff, 0x00, 0x00, 0x00};
  data[1] = (uint8_t)lamp;
  data[2] = on ? 0x80 : 0x00;

  Serial.printf("Lampe %d: %s wird gesendet.\n", lamp, on ? "EIN" : "AUS");
  return enqueueControl(data, 6);
}

bool BRMesh::setColor(int lamp, uint8_t r, uint8_t g, uint8_t b) {
  if (!isValidLamp(lamp)) return sendColorAndBrightness(lamp);  // Fehlermeldung
  const int idx = lamp - 1;
  red_[idx] = r;
  green_[idx] = g;
  blue_[idx] = b;
  return sendColorAndBrightness(lamp);
}

bool BRMesh::setBrightness(int lamp, uint8_t brightness) {
  if (!isValidLamp(lamp)) return sendColorAndBrightness(lamp);  // Fehlermeldung
  brightness_[lamp - 1] = brightness;
  return sendColorAndBrightness(lamp);
}

bool BRMesh::setColorAndBrightness(int lamp, uint8_t r, uint8_t g, uint8_t b,
                                   uint8_t brightness) {
  if (!isValidLamp(lamp)) return sendColorAndBrightness(lamp);  // Fehlermeldung
  const int idx = lamp - 1;
  red_[idx] = r;
  green_[idx] = g;
  blue_[idx] = b;
  brightness_[idx] = brightness;
  return sendColorAndBrightness(lamp);
}

bool BRMesh::setWhite(int lamp, uint8_t w6, uint8_t w7) {
  if (!isValidLamp(lamp)) return sendColorAndBrightness(lamp);  // Fehlermeldung
  white6_[lamp - 1] = w6;
  white7_[lamp - 1] = w7;
  return sendColorAndBrightness(lamp);
}

bool BRMesh::setLamp(int lamp, uint8_t r, uint8_t g, uint8_t b,
                     uint8_t brightness, uint8_t w6, uint8_t w7) {
  if (!isValidLamp(lamp)) return sendColorAndBrightness(lamp);  // Fehlermeldung
  const int idx = lamp - 1;
  red_[idx] = r;
  green_[idx] = g;
  blue_[idx] = b;
  brightness_[idx] = brightness;
  white6_[idx] = w6;
  white7_[idx] = w7;
  return sendColorAndBrightness(lamp);
}

bool BRMesh::sendColorAndBrightness(int lamp) {
  if (!isValidLamp(lamp)) {
    Serial.printf("Fehler: Lampennummer muss zwischen 1 und %u liegen.\n",
                  lampCount_);
    return false;
  }

  const int idx = lamp - 1;
  uint8_t data[] = {
      0x72,
      (uint8_t)lamp,
      (uint8_t)(0x80 | brightnessToProtocol(brightness_[idx])),  // On-Bit + 7-Bit Helligkeit
      blue_[idx],
      red_[idx],
      green_[idx],
      white6_[idx],   // Warm
      white7_[idx]};  // Kalt

  //Serial.printf(
  //    "Lampe %d: RGB(%u, %u, %u), W(%u, %u), Helligkeit %u/255 (Protokoll %u/127) wird gesendet.\n",
  //    lamp, red_[idx], green_[idx], blue_[idx], white6_[idx], white7_[idx],
  //    brightness_[idx], data[2] & 0x7F);

  return enqueueControl(data, sizeof(data));
}

bool BRMesh::setAll(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness,
                    uint16_t /*delayBetweenMs*/, uint8_t w6, uint8_t w7) {
  for (uint8_t i = 0; i < MAX_LAMPS; i++) {
    red_[i] = r;
    green_[i] = g;
    blue_[i] = b;
    brightness_[i] = brightness;
    white6_[i] = w6;
    white7_[i] = w7;
  }
  bool ok = true;
  for (int lamp = 1; lamp <= lampCount_; lamp++) {
    ok = sendColorAndBrightness(lamp) && ok;
  }
  return ok;
}

uint8_t BRMesh::red(int lamp) const {
  return isValidLamp(lamp) ? red_[lamp - 1] : 0;
}
uint8_t BRMesh::green(int lamp) const {
  return isValidLamp(lamp) ? green_[lamp - 1] : 0;
}
uint8_t BRMesh::blue(int lamp) const {
  return isValidLamp(lamp) ? blue_[lamp - 1] : 0;
}
uint8_t BRMesh::brightness(int lamp) const {
  return isValidLamp(lamp) ? brightness_[lamp - 1] : 0;
}
uint8_t BRMesh::white6(int lamp) const {
  return isValidLamp(lamp) ? white6_[lamp - 1] : 0;
}
uint8_t BRMesh::white7(int lamp) const {
  return isValidLamp(lamp) ? white7_[lamp - 1] : 0;
}

// --- Protokoll -------------------------------------------------------------

uint8_t BRMesh::packageBleFastconBody(int i, int i2, uint8_t sequence,
                                      uint8_t safeKey, int forward,
                                      const uint8_t *data, int length,
                                      uint8_t *&payload) {
  if (length > 12) {
    printf("data too long");
    payload = nullptr;
    return 0;
  }

  uint8_t payloadLength = 4 + 12;
  payload = (uint8_t *)malloc(payloadLength);
  if (!payload) return 0;

  payload[0] = ((i2 & 0b1111) << 0) | ((i & 0b111) << 4) | ((forward & 0xff) << 7);
  payload[1] = sequence & 0xff;
  payload[2] = safeKey;
  payload[3] = 0;

  for (int j = 4; j < payloadLength; j++) payload[j] = 0;
  memcpy(payload + 4, data, length);

  uint8_t checksum = 0;
  for (int j = 0; j < length + 4; j++) {
    if (j == 3) continue;
    checksum = (checksum + payload[j]) & 0xff;
  }
  payload[3] = checksum;

  for (int j = 0; j < 4; j++) payload[j] = DEFAULT_KEY[j & 3] ^ payload[j];
  for (int j = 0; j < 12; j++) payload[4 + j] = key_[j & 3] ^ payload[4 + j];

  return payloadLength;
}

uint8_t BRMesh::getPayloadWithInnerRetry(int i, const uint8_t *data, int length,
                                         int i2, int forward,
                                         uint8_t *&payload) {
  if (verbose_) {
    printf("data: ");
    dump(data, length);
    printf("\n");
  }

  sendCount_++;
  sendSeq_ = sendCount_;
  uint8_t safeKey = key_[3];

  return packageBleFastconBody(i, i2, sendSeq_, safeKey, forward, data, length,
                               payload);
}

uint8_t BRMesh::doGenerateCommand(int i, const uint8_t *data, uint8_t length,
                                  int forward, int i2, uint8_t *&rfPayload) {
  if (i2 < 0) i2 = 0;

  uint8_t *payload = nullptr;
  uint8_t *rfPayloadTmp = nullptr;
  uint8_t payloadLength =
      getPayloadWithInnerRetry(i, data, length, i2, forward, payload);
  if (!payloadLength || !payload) return 0;

  uint8_t rfPayloadLength =
      getRfPayload(DEFAULT_BLE_FASTCON_ADDRESS, payload, payloadLength, rfPayloadTmp);
  free(payload);
  if (!rfPayloadLength || !rfPayloadTmp) return 0;

  uint8_t ctx[7];
  whiteningInit(0x25, ctx);

  uint8_t *result = (uint8_t *)malloc(rfPayloadLength);
  if (!result) {
    free(rfPayloadTmp);
    return 0;
  }

  whiteningEncode(rfPayloadTmp, rfPayloadLength, ctx, result);
  rfPayload = (uint8_t *)malloc(rfPayloadLength - 15);

  if (!rfPayload) {
    free(result);
    free(rfPayloadTmp);
    return 0;
  }

  memcpy(rfPayload, result + 15, rfPayloadLength - 15);
  free(result);
  free(rfPayloadTmp);
  return rfPayloadLength - 15;
}
