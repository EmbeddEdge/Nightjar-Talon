#ifndef BLE_MANAGER_H
#define BLE_MANAGER_H

#include <Arduino.h>
#include <ArduinoBLE.h>
#include "config.h"

class BleManager {
public:
  BleManager();
  bool begin();
  void update(float currentLux, const char* surveyJson);
  
  bool isConnected() const { return _connected; }
  
  // Custom Service Interface
  bool hasControlCommand();
  String getControlCommand();
  
  // NUS Terminal Interface
  bool hasNusInput();
  String getNusInput();
  void sendNusOutput(const char* output);

private:
  bool _connected;
  
  // Custom Services & Characteristics
  BLEService _customService;
  BLEFloatCharacteristic _luxChar;
  BLECharacteristic _surveyStatusChar;
  BLECharacteristic _controlChar;
  
  // Nordic UART Service (NUS)
  BLEService _nusService;
  BLECharacteristic _nusRxChar;
  BLECharacteristic _nusTxChar;
  
  // Receive buffers
  bool _controlCommandAvailable;
  String _controlCommand;
  
  bool _nusInputAvailable;
  String _nusInput;
};

#endif // BLE_MANAGER_H
