/*******************************************************************************
 * Title                 :   Nightjar-Talon
 * Filename              :   main.cpp
 * Author                :   Turyn Lim Banda
 * Origin Date           :   12/08/2026

 * Version               :   2.0.0
 * Compiler              :   PlatformIO
 * Target                :   Arduino Nano 33 BLE Sense
 * Notes                 :   Onboard sensor integration for Lux surveying.
 *******************************************************************************/

#include "BleManager.h"
#include "CliManager.h"
#include "SensorManager.h"
#include "SurveyManager.h"
#include "config.h"
#include <Arduino.h>

// Instantiate core modules
SensorManager sensor;
SurveyManager survey;
BleManager ble;
CliManager cli(sensor, survey, ble);

// LED blink helper for fatal errors on boot
void fatalErrorBlink() {
  pinMode(LED_BUILTIN, OUTPUT);
  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }
}

void setup() {
  // Initialize USB Serial Monitor
  Serial.begin(SERIAL_MONITOR_BAUD);

  // Wait up to 1.5 seconds for Serial Monitor to connect (useful when debugging
  // via PC), but don't block indefinitely so the board can run on a
  // battery/power bank in the field.
  unsigned long start = millis();
  while (!Serial && (millis() - start < 1500)) {
    delay(10);
  }

  // Initialize APDS-9960 Light Sensor
  if (!sensor.begin()) {
    Serial.println("FATAL: Failed to initialize APDS-9960 light sensor!");
    fatalErrorBlink();
  }

  // Initialize Bluetooth Low Energy
  if (!ble.begin()) {
    Serial.println("FATAL: Failed to initialize ArduinoBLE!");
    fatalErrorBlink();
  }

  // Start Command Line Interface
  cli.begin();
}

void loop() {
  // Update sensor readings and auto-gain adjustments
  sensor.update();

  // Generate survey state JSON
  char surveyJson[256];
  survey.generateReportJson(surveyJson, sizeof(surveyJson));

  // Poll BLE and push notifications
  ble.update(sensor.getLux(), surveyJson);

  // Process incoming dashboard BLE Control commands
  if (ble.hasControlCommand()) {
    String cmd = ble.getControlCommand();

#if DEBUG
    Serial.print("BLE Command: ");
    Serial.println(cmd);
#endif

    if (cmd.startsWith("START:")) {
      String roomTypeId = cmd.substring(6);
      survey.startSurvey(roomTypeId.c_str());
      const RoomStandard *std = survey.getActiveRoomStandard();
      if (std) {
        cli.printOutput(
            "BLE_EVENT: Started survey for '%s' (Target: %.1f Lux)\n",
            std->name, std->targetLux);
      }
    } else if (cmd.startsWith("ADD:")) {
      String label = cmd.substring(4);
      if (survey.isActive()) {
        float currentLux = sensor.getLux();
        if (survey.addPoint(label.c_str(), currentLux)) {
          cli.printOutput("BLE_EVENT: Logged point '%s' = %.1f Lux\n",
                          label.c_str(), currentLux);
        }
      }
    } else if (cmd == "RESET" || cmd == "CLEAR") {
      survey.reset();
      cli.printOutput("BLE_EVENT: Survey reset\n");
    } else if (cmd == "END") {
      if (survey.isActive()) {
        cli.printOutput("BLE_EVENT: Survey finalized\n");
        survey.endSurvey();
      }
    }
  }

  // Poll and process CLI input (Serial and BLE NUS)
  cli.update();
}
