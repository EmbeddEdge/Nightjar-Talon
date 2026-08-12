#include "BleManager.h"

BleManager::BleManager()
    : _connected(false),
      _customService(BLE_SERVICE_UUID),
      _luxChar(BLE_CHAR_LUX_UUID, BLERead | BLENotify),
      _surveyStatusChar(BLE_CHAR_SURVEY_STATUS_UUID, BLERead | BLENotify, 256),
      _controlChar(BLE_CHAR_CONTROL_UUID, BLEWrite, 64),
      _nusService(BLE_NUS_SERVICE_UUID),
      _nusRxChar(BLE_NUS_CHAR_RX_UUID, BLEWrite | BLEWriteWithoutResponse, 64),
      _nusTxChar(BLE_NUS_CHAR_TX_UUID, BLENotify, 64),
      _controlCommandAvailable(false),
      _controlCommand(""),
      _nusInputAvailable(false),
      _nusInput("") {}

bool BleManager::begin() {
  if (!BLE.begin()) {
    return false;
  }

  BLE.setLocalName(DEVICE_NAME);
  
  // Set the Custom Service UUID as the advertised service so clients can scan for it
  BLE.setAdvertisedService(_customService);

  // Add custom service characteristics
  _customService.addCharacteristic(_luxChar);
  _customService.addCharacteristic(_surveyStatusChar);
  _customService.addCharacteristic(_controlChar);

  // Add NUS service characteristics
  _nusService.addCharacteristic(_nusRxChar);
  _nusService.addCharacteristic(_nusTxChar);

  // Add services to the BLE stack
  BLE.addService(_customService);
  BLE.addService(_nusService);

  // Set initial characteristic values
  _luxChar.writeValue(0.0f);
  _surveyStatusChar.writeValue("{}");

  // Start advertising
  BLE.advertise();

  return true;
}

void BleManager::update(float currentLux, const char* surveyJson) {
  // Poll BLE stack to process events
  BLE.poll();

  bool currentlyConnected = BLE.connected();
  if (currentlyConnected != _connected) {
    _connected = currentlyConnected;
    if (_connected) {
      #if DEBUG
      Serial.println("BLE Client Connected.");
      #endif
    } else {
      #if DEBUG
      Serial.println("BLE Client Disconnected. Re-advertising...");
      #endif
      BLE.advertise(); // Restart advertising when client disconnects
    }
  }

  // Update characteristic values if connected
  if (_connected) {
    _luxChar.writeValue(currentLux);
    _surveyStatusChar.writeValue(surveyJson);
  }

  // Check if dashboard commands were received
  if (_controlChar.written()) {
    int len = _controlChar.valueLength();
    const uint8_t* val = _controlChar.value();
    _controlCommand = "";
    for (int i = 0; i < len; i++) {
      _controlCommand += (char)val[i];
    }
    _controlCommandAvailable = true;
  }

  // Check if NUS Terminal commands were received
  if (_nusRxChar.written()) {
    int len = _nusRxChar.valueLength();
    const uint8_t* val = _nusRxChar.value();
    _nusInput = "";
    for (int i = 0; i < len; i++) {
      _nusInput += (char)val[i];
    }
    _nusInputAvailable = true;
  }
}

bool BleManager::hasControlCommand() {
  return _controlCommandAvailable;
}

String BleManager::getControlCommand() {
  _controlCommandAvailable = false;
  return _controlCommand;
}

bool BleManager::hasNusInput() {
  return _nusInputAvailable;
}

String BleManager::getNusInput() {
  _nusInputAvailable = false;
  return _nusInput;
}

void BleManager::sendNusOutput(const char* output) {
  if (!_connected) return;

  int len = strlen(output);
  int offset = 0;
  
  // Chunk transmissions into 20-byte blocks (BLE standard MTU limits payload compatibility)
  while (offset < len) {
    int chunkSize = len - offset;
    if (chunkSize > 20) {
      chunkSize = 20;
    }
    
    _nusTxChar.writeValue((const uint8_t*)&output[offset], chunkSize);
    offset += chunkSize;
    
    // Tiny delay to let the stack push the notify packet without dropouts
    delay(4);
  }
}
