#include "History.h"

bool tryUpdateMax(float& stored, float candidate, float eps) {
  if (candidate > stored + eps) {
    stored = candidate;
    return true;
  }
  return false;
}
