#ifndef CLI_MANAGER_H
#define CLI_MANAGER_H

#include "SensorManager.h"
#include "SurveyManager.h"
#include "BleManager.h"

class CliManager {
public:
  CliManager(SensorManager &sensor, SurveyManager &survey, BleManager &ble);
  void begin();
  void update();
  
  void printOutput(const char* format, ...);

private:
  SensorManager &_sensor;
  SurveyManager &_survey;
  BleManager &_ble;
  
  String _serialInput;
  String _bleInput;
  
  void processCommand(const String &cmdLine, bool fromBle);
  void showHelp();
  void showStatus();
  void showLux();
  
  // Helpers
  void splitCommand(const String &cmdLine, String &cmd, String &arg1, String &arg2);
};

#endif // CLI_MANAGER_H
