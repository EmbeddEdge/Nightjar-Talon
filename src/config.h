#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// BLE Configuration
// Custom Lux & Survey Service UUIDs
#define BLE_SERVICE_UUID                "19B10000-E8F2-537E-4F6C-D104768A1214"
#define BLE_CHAR_LUX_UUID               "19B10001-E8F2-537E-4F6C-D104768A1214"
#define BLE_CHAR_SURVEY_STATUS_UUID     "19B10002-E8F2-537E-4F6C-D104768A1214"
#define BLE_CHAR_CONTROL_UUID           "19B10003-E8F2-537E-4F6C-D104768A1214"

// Nordic UART Service (NUS) UUIDs for wireless CLI Terminal support
#define BLE_NUS_SERVICE_UUID            "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_NUS_CHAR_RX_UUID            "6E400002-B5A3-F393-E0A9-E50E24DCCA9E" // Write/WriteWithoutResponse (Client -> Arduino)
#define BLE_NUS_CHAR_TX_UUID            "6E400003-B5A3-F393-E0A9-E50E24DCCA9E" // Notify (Arduino -> Client)

// Sensor & System settings
#define SENSOR_READ_INTERVAL_MS         250      // Rate to read raw values & adjust auto-gain
#define SERIAL_MONITOR_BAUD             115200
#define FIRMWARE_VERSION                "1.0.0"
#define DEVICE_NAME                     "Nightjar Talon"

// Room Illumination Standards (Target Lux based on CIBSE/IES recommendations)
struct RoomStandard {
  const char* id;
  const char* name;
  float targetLux;
};

const RoomStandard ROOM_STANDARDS[] = {
  {"office",    "Office / Work Desk",   500.0f},
  {"kitchen",   "Kitchen Counters",     350.0f},
  {"living",    "Living / Family Room", 150.0f},
  {"bedroom",   "Bedroom",              120.0f},
  {"dining",    "Dining Room",          150.0f},
  {"bathroom",  "Bathroom",             200.0f},
  {"hallway",   "Hallway / Corridor",   80.0f},
  {"workshop",  "Workshop / Garage",    300.0f}
};

const int NUM_ROOM_STANDARDS = sizeof(ROOM_STANDARDS) / sizeof(ROOM_STANDARDS[0]);

inline const RoomStandard* getRoomStandardById(const char* id) {
  for (int i = 0; i < NUM_ROOM_STANDARDS; i++) {
    if (strcasecmp(ROOM_STANDARDS[i].id, id) == 0) {
      return &ROOM_STANDARDS[i];
    }
  }
  return nullptr;
}

#endif // CONFIG_H
