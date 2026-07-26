#include "SelfDiag.h"
#include <cstdio>

void formatSelfDiagLine(char* out, size_t outSize, uint32_t freeHeapBytes,
                         uint16_t loopHz) {
  std::snprintf(out, outSize, "Heap %luk %uHz",
                static_cast<unsigned long>(freeHeapBytes / 1024), loopHz);
}
