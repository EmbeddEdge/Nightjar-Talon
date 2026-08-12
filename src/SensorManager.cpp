#include "SensorManager.h"
#include <Arduino_APDS9960.h>
#include <Wire.h>

#ifdef ARDUINO_ARDUINO_NANO33BLE
#define WIRE_PORT Wire1
#else
#define WIRE_PORT Wire
#endif

#define APDS9960_I2C_ADDR 0x39
#define APDS9960_REG_ATIME 0x81
#define APDS9960_REG_CONTROL 0x8F

SensorManager::SensorManager()
    : _lux(0.0f), _colorTemp(0.0f), _gain(4), _gainRegisterVal(1),
      _atime(252), _integrationTimeMs(11.12f), _saturated(false),
      _calibrationFactor(1.0f), _lastReadTime(0) {
  _rgbc = {0, 0, 0, 0};
}

bool SensorManager::begin() {
  // APDS.begin() automatically calls WIRE_PORT.begin() internally
  if (!APDS.begin()) {
    return false;
  }

  // Read initial registers to sync our state
  _atime = readRegister(APDS9960_REG_ATIME);
  uint8_t control = readRegister(APDS9960_REG_CONTROL);
  _gainRegisterVal = control & 0x03;

  // Map register val to gain factor
  setGain(_gainRegisterVal);
  _integrationTimeMs = (256 - _atime) * 2.78f;

  _lastReadTime = millis();
  _saturated = false;
  return true;
}

void SensorManager::update() {
  unsigned long now = millis();
  if (now - _lastReadTime < 250) {
    return;
  }
  _lastReadTime = now;

  if (APDS.colorAvailable()) {
    int r, g, b, c;
    APDS.readColor(r, g, b, c);

    _rgbc.r = r;
    _rgbc.g = g;
    _rgbc.b = b;
    _rgbc.c = c;

    // Run software auto-ranging
    adjustGain(c);

    // Calculate actual lux and color temperature using normalized values
    calculateLuxAndColorTemp();
  }
}

uint8_t SensorManager::readRegister(uint8_t reg) {
  WIRE_PORT.beginTransmission(APDS9960_I2C_ADDR);
  WIRE_PORT.write(reg);
  if (WIRE_PORT.endTransmission(false) != 0) {
    return 0; // Error
  }
  WIRE_PORT.requestFrom(APDS9960_I2C_ADDR, 1);
  if (WIRE_PORT.available()) {
    return WIRE_PORT.read();
  }
  return 0;
}

void SensorManager::writeRegister(uint8_t reg, uint8_t val) {
  WIRE_PORT.beginTransmission(APDS9960_I2C_ADDR);
  WIRE_PORT.write(reg);
  WIRE_PORT.write(val);
  WIRE_PORT.endTransmission();
}

void SensorManager::setGain(uint8_t gainRegisterVal) {
  _gainRegisterVal = gainRegisterVal;
  switch (_gainRegisterVal) {
    case 0: _gain = 1; break;
    case 1: _gain = 4; break;
    case 2: _gain = 16; break;
    case 3: _gain = 64; break;
    default: _gain = 4; _gainRegisterVal = 1; break;
  }
}

void SensorManager::adjustGain(int clearVal) {
  // Max count depends on ATIME. For standard ATIME = 252 (11.12ms), max is 4096.
  // We threshold based on this ceiling.
  int maxCount = (256 - _atime) * 1024;
  if (maxCount > 65535) maxCount = 65535;

  int upperThreshold = (int)(maxCount * 0.90f); // 90% of max
  int lowerThreshold = (int)(maxCount * 0.10f); // 10% of max

  bool changed = false;

  if (clearVal >= upperThreshold && _gainRegisterVal > 0) {
    // Decrease gain to avoid saturation
    _gainRegisterVal--;
    changed = true;
  } else if (clearVal < lowerThreshold && _gainRegisterVal < 3) {
    // Increase gain to get better low-light resolution
    _gainRegisterVal++;
    changed = true;
  }

  // Set saturation flag
  _saturated = (clearVal >= upperThreshold && _gainRegisterVal == 0);

  if (changed) {
    // Read-modify-write CONTROL register
    uint8_t control = readRegister(APDS9960_REG_CONTROL);
    control &= ~0x03; // Clear bits 1:0
    control |= (_gainRegisterVal & 0x03);
    writeRegister(APDS9960_REG_CONTROL, control);

    // Update gain factor
    setGain(_gainRegisterVal);
  }
}

void SensorManager::calculateLuxAndColorTemp() {
  // Normalize counts to reference setting: 1x Gain, 71.2ms integration time
  // This scales the counts linearly so we can apply standard coefficients.
  float scale = 71.2f / (_integrationTimeMs * _gain);
  float r_norm = _rgbc.r * scale;
  float g_norm = _rgbc.g * scale;
  float b_norm = _rgbc.b * scale;
  float c_norm = _rgbc.c * scale;

  // Calculate CIE 1931 XYZ tristimulus values using typical APDS-9960 coefficients
  float X = (-0.14282f * r_norm) + (1.54924f * g_norm) + (-0.95641f * b_norm);
  float Y = (-0.32466f * r_norm) + (1.57837f * g_norm) + (-0.73191f * b_norm);
  float Z = (-0.68202f * r_norm) + (0.77073f * g_norm) + (0.56332f * b_norm);

  // CIE Photopic Luminance Y directly corresponds to Lux
  _lux = Y;

  // Fallback to Clear channel approximation if Lux is calculated as negative/unstable
  if (_lux < 0.1f) {
    _lux = c_norm * 0.14f;
    if (_lux < 0.0f) _lux = 0.0f;
  }

  _lux *= _calibrationFactor;

  // Calculate Chromaticity coordinates (x, y) for Correlated Color Temperature (CCT)
  float sum = X + Y + Z;
  if (sum > 0.0f) {
    float x = X / sum;
    float y = Y / sum;

    // Apply McCamy's cubic approximation formula
    float n = (x - 0.3320f) / (0.1858f - y);
    _colorTemp = (449.0f * n * n * n) + (3525.0f * n * n) + (6823.3f * n) + 5520.33f;

    // Clamp unrealistic values
    if (_colorTemp < 1000.0f || _colorTemp > 25000.0f) {
      _colorTemp = 0.0f;
    }
  } else {
    _colorTemp = 0.0f;
  }
}
