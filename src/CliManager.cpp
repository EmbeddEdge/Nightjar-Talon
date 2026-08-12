#include "CliManager.h"
#include <stdarg.h>

CliManager::CliManager(SensorManager &sensor, SurveyManager &survey, BleManager &ble)
    : _sensor(sensor), _survey(survey), _ble(ble), _serialInput(""), _bleInput("") {}

void CliManager::begin() {
  // Print initial greeting on boot
  Serial.println("==========================================");
  Serial.println("   Nightjar-Talon Light Sensor Platform   ");
  Serial.print("   Version: "); Serial.println(FIRMWARE_VERSION);
  Serial.println("   Type 'help' for commands list          ");
  Serial.println("==========================================");
}

void CliManager::update() {
  // 1. Process USB Serial console input
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (_serialInput.length() > 0) {
        processCommand(_serialInput, false);
        _serialInput = "";
      }
    } else if (c >= 32 && c <= 126) {
      _serialInput += c;
    }
  }

  // 2. Process BLE NUS Console input
  if (_ble.hasNusInput()) {
    String input = _ble.getNusInput();
    for (unsigned int i = 0; i < input.length(); i++) {
      char c = input[i];
      if (c == '\n' || c == '\r') {
        if (_bleInput.length() > 0) {
          processCommand(_bleInput, true);
          _bleInput = "";
        }
      } else if (c >= 32 && c <= 126) {
        _bleInput += c;
      }
    }
  }
}

void CliManager::printOutput(const char* format, ...) {
  char buffer[256];
  va_list args;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);

  // Print to USB Serial
  Serial.print(buffer);

  // Print to BLE NUS (if connected)
  _ble.sendNusOutput(buffer);
}

void CliManager::splitCommand(const String &cmdLine, String &cmd, String &arg1, String &arg2) {
  cmd = "";
  arg1 = "";
  arg2 = "";

  int index = 0;
  int len = cmdLine.length();

  // Trim leading whitespace
  while (index < len && cmdLine[index] == ' ') index++;

  // Get command word
  while (index < len && cmdLine[index] != ' ') {
    cmd += cmdLine[index];
    index++;
  }

  // Trim middle whitespace
  while (index < len && cmdLine[index] == ' ') index++;

  // Get arg1 word
  while (index < len && cmdLine[index] != ' ') {
    arg1 += cmdLine[index];
    index++;
  }

  // Trim middle whitespace
  while (index < len && cmdLine[index] == ' ') index++;

  // Get arg2 (rest of line)
  while (index < len) {
    arg2 += cmdLine[index];
    index++;
  }

  cmd.trim();
  arg1.trim();
  arg2.trim();
}

void CliManager::processCommand(const String &cmdLine, bool fromBle) {
  #if DEBUG
  // Echo command back to local USB for logging
  if (fromBle) {
    Serial.print("BLE-CLI: ");
    Serial.println(cmdLine);
  }
  #endif

  String cmd, arg1, arg2;
  splitCommand(cmdLine, cmd, arg1, arg2);
  cmd.toLowerCase();

  if (cmd == "help" || cmd == "?") {
    showHelp();
  } else if (cmd == "status") {
    showStatus();
  } else if (cmd == "lux") {
    showLux();
  } else if (cmd == "survey") {
    arg1.toLowerCase();
    if (arg1 == "start") {
      if (arg2 == "") {
        arg2 = "office"; // Default standard
      }
      _survey.startSurvey(arg2.c_str());
      const RoomStandard* std = _survey.getActiveRoomStandard();
      if (std) {
        printOutput("SUCCESS: Survey started for '%s' (Target: %.1f Lux)\n", std->name, std->targetLux);
      } else {
        printOutput("ERROR: Failed to start survey.\n");
      }
    } else if (arg1 == "add") {
      if (!_survey.isActive()) {
        printOutput("ERROR: No active survey. Start one using 'survey start <room_type>'\n");
        return;
      }
      if (arg2 == "") {
        char defaultLabel[16];
        snprintf(defaultLabel, sizeof(defaultLabel), "Point_%d", _survey.getPointCount() + 1);
        arg2 = defaultLabel;
      }
      float currentLux = _sensor.getLux();
      if (_survey.addPoint(arg2.c_str(), currentLux)) {
        printOutput("RECORDED: Point '%s' = %.1f Lux\n", arg2.c_str(), currentLux);
      } else {
        printOutput("ERROR: Failed to add point (survey full or inactive).\n");
      }
    } else if (arg1 == "end") {
      if (!_survey.isActive()) {
        printOutput("ERROR: No active survey is running.\n");
        return;
      }

      // Generate a detailed ASCII survey report
      const RoomStandard* std = _survey.getActiveRoomStandard();
      int count = _survey.getPointCount();
      float avg = _survey.getAverageLux();
      float minLux = _survey.getMinLux();
      float maxLux = _survey.getMaxLux();
      float uniformity = _survey.getUniformity();
      float target = std ? std->targetLux : 100.0f;
      bool meetsAvg = avg >= target;
      bool meetsUnif = uniformity >= 0.40f;

      printOutput("\n============================================\n");
      printOutput("          LIGHTING SURVEY REPORT            \n");
      printOutput("============================================\n");
      printOutput("Room Standard : %s\n", std ? std->name : "Custom");
      printOutput("Target Lux    : %.1f Lux\n", target);
      printOutput("--------------------------------------------\n");
      printOutput("Recorded measurements (%d):\n", count);
      
      for (int i = 0; i < count; i++) {
        SurveyPoint pt = _survey.getPoint(i);
        bool pointPass = pt.lux >= target;
        printOutput(" - %-12s: %6.1f Lux  [%s]\n", pt.label, pt.lux, pointPass ? "PASS" : "LOW");
      }
      
      printOutput("--------------------------------------------\n");
      printOutput("Statistical Summary:\n");
      printOutput(" - Average Lux   : %.1f Lux  [%s]\n", avg, meetsAvg ? "SATISFACTORY" : "DEFICIENT");
      printOutput(" - Minimum Lux   : %.1f Lux\n", minLux);
      printOutput(" - Maximum Lux   : %.1f Lux\n", maxLux);
      printOutput(" - Uniformity    : %.2f       [%s]\n", uniformity, meetsUnif ? "COMPLIANT" : "POOR");
      printOutput("--------------------------------------------\n");
      printOutput("Compliance Status: %s\n", (meetsAvg && meetsUnif) ? "PASSED" : "FAILED");
      printOutput("--------------------------------------------\n");
      printOutput("Recommendations & Actions:\n");

      if (!meetsAvg) {
        float deficit = target - avg;
        printOutput(" * DEFICIT: Room is under-illuminated by %.1f Lux on average.\n", deficit);
        printOutput(" * FIREFIGHTING: Increase primary light fixtures to add more lumens.\n");
      } else {
        printOutput(" * Average ambient light levels meet the room requirements.\n");
      }

      if (!meetsUnif) {
        printOutput(" * UNIFORMITY: Light distribution is uneven (%.2f < 0.40).\n", uniformity);
        printOutput(" * REMEDY: Space lighting sources more evenly or add diffuse fixtures.\n");
      }

      // Check for individual dark spots (less than 70% of target)
      bool darkSpotsFound = false;
      for (int i = 0; i < count; i++) {
        SurveyPoint pt = _survey.getPoint(i);
        if (pt.lux < target * 0.70f) {
          if (!darkSpotsFound) {
            printOutput(" * DARK SPOTS IDENTIFIED:\n");
            darkSpotsFound = true;
          }
          printOutput("   - Area around '%s' is very dark (%.1f Lux).\n", pt.label, pt.lux);
          printOutput("     Recommendation: Place a task lamp (table lamp, spot) here.\n");
        }
      }

      if (meetsAvg && meetsUnif && !darkSpotsFound) {
        printOutput(" * Excellent! The lighting layout is optimal and fully compliant.\n");
      }

      printOutput("============================================\n\n");

      _survey.endSurvey();
    } else if (arg1 == "clear" || arg1 == "reset") {
      _survey.reset();
      printOutput("SUCCESS: Survey data reset.\n");
    } else {
      printOutput("ERROR: Unknown survey subcommand. Use help to see list.\n");
    }
  } else if (cmd == "calibrate") {
    if (arg1 == "") {
      printOutput("Calibration Factor: %.2f\n", _sensor.getCalibrationFactor());
    } else {
      float val = arg1.toFloat();
      if (val > 0.0f) {
        _sensor.setCalibrationFactor(val);
        printOutput("SUCCESS: Calibration factor set to %.2f\n", val);
      } else {
        printOutput("ERROR: Factor must be positive.\n");
      }
    }
  } else {
    printOutput("ERROR: Unknown command '%s'. Type 'help' for command list.\n", cmd.c_str());
  }
}

void CliManager::showHelp() {
  printOutput("\n--- Available Commands ---\n");
  printOutput("help                   : Show this command list\n");
  printOutput("status                 : Print system state and active configurations\n");
  printOutput("lux                    : Read real-time lux, CCT, and raw colors\n");
  printOutput("calibrate <factor>     : Read or set calibration scale multiplier\n");
  printOutput("survey start <room>    : Start a room survey (office, kitchen, living, bedroom, hallway)\n");
  printOutput("survey add <label>     : Record sensor reading at specific point label\n");
  printOutput("survey end             : Complete survey and display compliance analysis report\n");
  printOutput("survey clear           : Discard active survey data\n\n");
}

void CliManager::showStatus() {
  printOutput("\n--- System Status ---\n");
  printOutput("Firmware Version : %s\n", FIRMWARE_VERSION);
  printOutput("BLE Connection   : %s\n", _ble.isConnected() ? "Connected" : "Advertising / Idle");
  printOutput("Sensor Auto-Gain : Active (Current: %dx, IT: %.1f ms)\n", _sensor.getGain(), _sensor.getIntegrationTimeMs());
  printOutput("Sensor Saturation: %s\n", _sensor.isSaturated() ? "YES (Saturated!)" : "No (Normal)");
  printOutput("Survey Status    : %s\n", _survey.isActive() ? "ACTIVE" : "INACTIVE");
  if (_survey.isActive()) {
    const RoomStandard* std = _survey.getActiveRoomStandard();
    printOutput(" - Active Room   : %s\n", std ? std->name : "Custom");
    printOutput(" - Target Lux    : %.1f Lux\n", std ? std->targetLux : 0.0f);
    printOutput(" - Points Logged : %d\n", _survey.getPointCount());
  }
  printOutput("\n");
}

void CliManager::showLux() {
  RGBCData rgbc = _sensor.getRGBC();
  printOutput("\n--- Real-Time Light Readings ---\n");
  printOutput("Calculated Lux   : %.1f Lux\n", _sensor.getLux());
  printOutput("Color Temp (CCT) : %.0f K\n", _sensor.getColorTemp());
  printOutput("Raw RGBC Data    : R=%d, G=%d, B=%d, C=%d\n", rgbc.r, rgbc.g, rgbc.b, rgbc.c);
  printOutput("Current Gain     : %dx\n", _sensor.getGain());
  printOutput("\n");
}
