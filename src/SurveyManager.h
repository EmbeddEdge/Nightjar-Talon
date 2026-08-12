#ifndef SURVEY_MANAGER_H
#define SURVEY_MANAGER_H

#include "config.h"

#define MAX_SURVEY_POINTS 20

struct SurveyPoint {
  char label[16];
  float lux;
};

class SurveyManager {
public:
  SurveyManager();
  
  void startSurvey(const char* roomTypeId);
  bool addPoint(const char* label, float lux);
  void endSurvey();
  void reset();
  
  bool isActive() const { return _active; }
  const RoomStandard* getActiveRoomStandard() const { return _roomStandard; }
  int getPointCount() const { return _pointCount; }
  SurveyPoint getPoint(int index) const;
  
  float getAverageLux() const;
  float getMinLux() const;
  float getMaxLux() const;
  float getUniformity() const; // Min / Avg ratio
  bool isCompliant() const;
  
  void generateReportJson(char* buffer, int maxLen) const;

private:
  bool _active;
  const RoomStandard* _roomStandard;
  SurveyPoint _points[MAX_SURVEY_POINTS];
  int _pointCount;
  
  void calculateStats(float &avg, float &min, float &max, float &unif) const;
};

#endif // SURVEY_MANAGER_H
