/*
  SignBridge glove firmware (ESP32 DevKit V1, Arduino core)
  Streams raw sensor data to the SignBridge web app over Bluetooth LE. All recognition happens in the app.
  Four flex sensors for now: thumb, index, middle, ring. The pinky sensor was damaged and will be added back
  on VP (GPIO 36); bytes 8-9 of the packet are kept for it.

  Packet, 17 bytes, 50 per second, Nordic UART TX characteristic (notify):
    [0..7]   flex, uint16 little-endian, 12-bit ADC (0-4095): thumb, index, middle, ring
    [8..9]   pinky, reserved: always 0 until the pinky sensor is added back on VP (GPIO 36)
    [10..12] accel x, y, z   int8, 1 unit = 1/32 g   (+-2 g range -> +-64)
    [13..15] gyro  x, y, z   int8, 1 unit = 8 deg/s  (+-1000 deg/s range)
    [16]     flags: bit0 = button held, bit1 = MPU6050 found

  WIRING (ADC1 pins only, so Bluetooth/WiFi never disables them)
    Each flex sensor: 3V3 --[flex]--+-- pin
                                    +--[10k]-- GND
      thumb D32, index D33, middle D34, ring D35; pinky VP (GPIO 36) planned
    MPU6050: VCC 3V3, GND, SDA D21, SCL D22
    Button:  D27 -- button -- GND        (optional: starts a recording in the app)
    Status LED: on-board blue LED (GPIO 2): blinking = waiting for the app, solid = connected
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <Wire.h>

const int NF = 4;                                // number of flex sensors
const int FLEX_PINS[NF] = {32, 33, 34, 35};      // thumb, index, middle, ring (pinky later: NF = 5, add 36)
const int BUTTON_PIN = 27;
const int LED_PIN = 2;
int MPU_ADDR = 0x68;

#define SERVICE_UUID "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define TX_UUID      "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

BLECharacteristic* tx;
bool connected = false;
bool mpuFound = false;
float smooth[NF];
unsigned long lastSend = 0, lastPrint = 0;

class Callbacks : public BLEServerCallbacks {
  void onConnect(BLEServer*) { connected = true; }
  void onDisconnect(BLEServer* s) { connected = false; s->getAdvertising()->start(); }
};

void mpuWrite(byte reg, byte val) {
  Wire.beginTransmission(MPU_ADDR); Wire.write(reg); Wire.write(val); Wire.endTransmission();
}

// 16 quick reads, sorted, and the middle 8 averaged (a trimmed mean): cuts ADC noise and ignores the odd spike.
float readFlex(int pin) {
  int v[16];
  for (int k = 0; k < 16; k++) v[k] = analogRead(pin);
  for (int a = 1; a < 16; a++) { int x = v[a], b = a - 1; while (b >= 0 && v[b] > x) { v[b + 1] = v[b]; b--; } v[b + 1] = x; }
  long sum = 0;
  for (int k = 4; k < 12; k++) sum += v[k];
  return sum / 8.0;
}

// Adaptive smoothing: small changes (sensor jitter, slow creep) are smoothed heavily so the value sits still,
// while a real bend (a big jump) passes through quickly.
float smoothStep(float old, float now) {
  float d = fabsf(now - old);
  float a = d < 12 ? 0.08 : d < 40 ? 0.3 : 0.6;
  return old + (now - old) * a;
}

int8_t toI8(float v) { return (int8_t)constrain((int)lroundf(v), -127, 127); }

void setup() {
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);                 // full 0-3.3 V range

  Wire.begin(21, 22);
  Serial.print("I2C devices:");
  int found = 0;
  for (int a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { Serial.printf(" 0x%02X", a); found++; }
  }
  Serial.println(found ? "" : " none");
  for (int a : {0x68, 0x69}) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) { MPU_ADDR = a; mpuFound = true; break; }
  }
  if (mpuFound) {
    mpuWrite(0x6B, 0x00);     // wake up
    mpuWrite(0x1A, 0x03);     // 44 Hz low-pass filter, cuts vibration noise
    mpuWrite(0x1B, 0x10);     // gyro +-1000 deg/s
    mpuWrite(0x1C, 0x00);     // accel +-2 g
  }
  Serial.println(mpuFound ? "MPU6050 found" : "No MPU6050: motion data will be zero");

  for (int i = 0; i < NF; i++) smooth[i] = readFlex(FLEX_PINS[i]);

  BLEDevice::init("SignBridge Glove");
  BLEServer* server = BLEDevice::createServer();
  server->setCallbacks(new Callbacks());
  BLEService* service = server->createService(SERVICE_UUID);
  tx = service->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
  tx->addDescriptor(new BLE2902());
  service->start();
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->setScanResponse(true);                     // puts the name in the scan response
  adv->start();
  Serial.println("SignBridge glove ready");
}

void loop() {
  if (millis() - lastSend < 20) return;           // 50 packets per second
  lastSend = millis();

  for (int i = 0; i < NF; i++) smooth[i] = smoothStep(smooth[i], readFlex(FLEX_PINS[i]));

  float ax = 0, ay = 0, az = 0, gx = 0, gy = 0, gz = 0;
  if (mpuFound) {
    Wire.beginTransmission(MPU_ADDR); Wire.write(0x3B); Wire.endTransmission(false);
    if (Wire.requestFrom(MPU_ADDR, 14) == 14) {
      int16_t v[7];
      for (int i = 0; i < 7; i++) { v[i] = Wire.read() << 8; v[i] |= Wire.read(); }
      ax = v[0] / 512.0; ay = v[1] / 512.0; az = v[2] / 512.0;              // 1/32 g units
      gx = v[4] / 32.8 / 8; gy = v[5] / 32.8 / 8; gz = v[6] / 32.8 / 8;   // 8 deg/s units
    }
  }
  bool button = digitalRead(BUTTON_PIN) == LOW;

  uint8_t p[17];
  p[8] = p[9] = 0;                                // pinky slot, unused until the sensor is replaced
  for (int i = 0; i < NF; i++) {
    uint16_t f = (uint16_t)constrain((int)smooth[i], 0, 4095);
    p[i * 2] = f & 0xFF; p[i * 2 + 1] = f >> 8;
  }
  p[10] = toI8(ax); p[11] = toI8(ay); p[12] = toI8(az);
  p[13] = toI8(gx); p[14] = toI8(gy); p[15] = toI8(gz);
  p[16] = (button ? 1 : 0) | (mpuFound ? 2 : 0);

  if (connected) { tx->setValue(p, 17); tx->notify(); }
  digitalWrite(LED_PIN, connected ? HIGH : (millis() / 400) % 2);

  if (millis() - lastPrint > 200) {               // 5 lines a second for the Serial Monitor
    lastPrint = millis();
    Serial.printf("thumb %4d index %4d middle %4d ring %4d | acc %4d %4d %4d | gyro %4d %4d %4d | btn %d | %s\n",
      (int)smooth[0], (int)smooth[1], (int)smooth[2], (int)smooth[3],
      (int8_t)p[10], (int8_t)p[11], (int8_t)p[12], (int8_t)p[13], (int8_t)p[14], (int8_t)p[15],
      button, connected ? "app connected" : "advertising");
  }
}
