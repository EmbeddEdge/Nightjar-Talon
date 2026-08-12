#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>

struct RGBCData {
  int r;
  int g;
  int b;
  int c;
};

class SensorManager {
public:
  SensorManager();
  bool begin();
  void update();

  float getLux() const { return _lux; }
  float getColorTemp() const { return _colorTemp; }
  RGBCData getRGBC() const { return _rgbc; }
  uint8_t getGain() const { return _gain; } // 1, 4, 16, 64
  float getIntegrationTimeMs() const { return _integrationTimeMs; }
  bool isSaturated() const { return _saturated; }

  void setCalibrationFactor(float f) { _calibrationFactor = f; }
  float getCalibrationFactor() const { return _calibrationFactor; }

private:
  float _lux;
  float _colorTemp;
  RGBCData _rgbc;
  uint8_t _gain; // 1, 4, 16, 64
  uint8_t _gainRegisterVal; // 0=1x, 1=4x, 2=16x, 3=64x
  uint8_t _atime;
  float _integrationTimeMs;
  bool _saturated;
  float _calibrationFactor;
  unsigned long _lastReadTime;

  // I2C helpers for register access
  uint8_t readRegister(uint8_t reg);
  void writeRegister(uint8_t reg, uint8_t val);
  
  // Software auto-ranging implementation
  void adjustGain(int clearVal);
  void setGain(uint8_t gainRegisterVal); // 0=1x, 1=4x, 2=16x, 3=64x
  
  // Computations
  void calculateLuxAndColorTemp();
};

#endif // SENSOR_MANAGER_H
