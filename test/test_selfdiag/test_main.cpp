// =============================================================
//  test_main.cpp — Unity-Tests fuer die SelfDiag-Formatierung.
// =============================================================
#include <cstring>
#include <unity.h>
#include "SelfDiag.h"

void setUp() {}
void tearDown() {}

static void test_format_self_diag_line_basic() {
  char buf[24];
  formatSelfDiagLine(buf, sizeof(buf), 142 * 1024, 45);
  TEST_ASSERT_EQUAL_STRING("Heap:142k | Hz:45 *", buf);
}

static void test_format_self_diag_line_truncates_safely() {
  char buf[8];
  formatSelfDiagLine(buf, sizeof(buf), 142 * 1024, 45);
  // Nullterminiert und nicht laenger als der Puffer -- kein Absturz,
  // kein Ueberlauf, auch wenn der Text dabei abgeschnitten wird.
  TEST_ASSERT_TRUE(std::strlen(buf) < sizeof(buf));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_format_self_diag_line_basic);
  RUN_TEST(test_format_self_diag_line_truncates_safely);
  return UNITY_END();
}
