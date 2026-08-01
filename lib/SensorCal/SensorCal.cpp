#include "SensorCal.h"

float dieTempC(float rawSondeC, float ambC, float k) {
  return rawSondeC + k * (rawSondeC - ambC);
}
