#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>
#include <time.h>

class Utils {
public:

  void setupPins();

  // ----------------------
// Time
// ----------------------
bool getTimeSafe(struct tm &t);

// ----------------------
// Display helpers
// ----------------------
void formatTemperature(float temp, char* out);
};

extern Utils utils;
#endif
