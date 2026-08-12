#include "SurveyManager.h"
#include <string.h>
#include <stdio.h>

SurveyManager::SurveyManager()
    : _active(false), _roomStandard(nullptr), _pointCount(0) {
  memset(_points, 0, sizeof(_points));
}

void SurveyManager::startSurvey(const char* roomTypeId) {
  reset();
  _roomStandard = getRoomStandardById(roomTypeId);
  if (!_roomStandard && NUM_ROOM_STANDARDS > 0) {
    // Fallback to the first standard (typically office)
    _roomStandard = &ROOM_STANDARDS[0];
  }
  _active = true;
}

bool SurveyManager::addPoint(const char* label, float lux) {
  if (!_active || _pointCount >= MAX_SURVEY_POINTS) {
    return false;
  }

  strncpy(_points[_pointCount].label, label, sizeof(_points[_pointCount].label) - 1);
  _points[_pointCount].label[sizeof(_points[_pointCount].label) - 1] = '\0';
  _points[_pointCount].lux = lux;
  _pointCount++;
  
  return true;
}

void SurveyManager::endSurvey() {
  _active = false;
}

void SurveyManager::reset() {
  _active = false;
  _roomStandard = nullptr;
  _pointCount = 0;
  memset(_points, 0, sizeof(_points));
}

SurveyPoint SurveyManager::getPoint(int index) const {
  if (index >= 0 && index < _pointCount) {
    return _points[index];
  }
  SurveyPoint dummy = {"", 0.0f};
  return dummy;
}

void SurveyManager::calculateStats(float &avg, float &min, float &max, float &unif) const {
  if (_pointCount == 0) {
    avg = 0.0f;
    min = 0.0f;
    max = 0.0f;
    unif = 0.0f;
    return;
  }

  float sum = 0.0f;
  min = _points[0].lux;
  max = _points[0].lux;

  for (int i = 0; i < _pointCount; i++) {
    sum += _points[i].lux;
    if (_points[i].lux < min) min = _points[i].lux;
    if (_points[i].lux > max) max = _points[i].lux;
  }

  avg = sum / _pointCount;
  unif = avg > 0.0f ? min / avg : 0.0f;
}

float SurveyManager::getAverageLux() const {
  float avg, min, max, unif;
  calculateStats(avg, min, max, unif);
  return avg;
}

float SurveyManager::getMinLux() const {
  float avg, min, max, unif;
  calculateStats(avg, min, max, unif);
  return min;
}

float SurveyManager::getMaxLux() const {
  float avg, min, max, unif;
  calculateStats(avg, min, max, unif);
  return max;
}

float SurveyManager::getUniformity() const {
  float avg, min, max, unif;
  calculateStats(avg, min, max, unif);
  return unif;
}

bool SurveyManager::isCompliant() const {
  if (!_active || _pointCount == 0) return false;
  float target = _roomStandard ? _roomStandard->targetLux : 100.0f;
  return (getAverageLux() >= target) && (getUniformity() >= 0.4f);
}

void SurveyManager::generateReportJson(char* buffer, int maxLen) const {
  float avg, min, max, unif;
  calculateStats(avg, min, max, unif);
  
  const char* roomId = _roomStandard ? _roomStandard->id : "none";
  float target = _roomStandard ? _roomStandard->targetLux : 0.0f;
  bool comp = (_pointCount > 0) && (avg >= target) && (unif >= 0.4f);

  snprintf(buffer, maxLen,
           "{\"active\":%s,\"room\":\"%s\",\"target\":%.1f,\"count\":%d,\"avg\":%.1f,\"min\":%.1f,\"max\":%.1f,\"unif\":%.2f,\"compliant\":%s}",
           _active ? "true" : "false", roomId, target, _pointCount, avg, min, max, unif, comp ? "true" : "false");
}
