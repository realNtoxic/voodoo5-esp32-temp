#include "SensorCal.h"
#include "config.h"

float applySensorOffset(float rawC, uint8_t sensorIndex) {
  return rawC + SENSOR_OFFSET_C[sensorIndex];
}
