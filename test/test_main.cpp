#include <cstdio>

#include "test_framework.h"

int main() {
  using namespace windclock_test;

  int passed = 0;
  int failed = 0;
  const char *currentSuite = nullptr;

  for (const TestCase &test : registry()) {
    if (currentSuite == nullptr ||
        std::string(currentSuite) != std::string(test.suite)) {
      currentSuite = test.suite;
      std::printf("\n%s\n", currentSuite);
    }

    const size_t before = failures().size();
    test.fn();
    const size_t after = failures().size();

    if (after == before) {
      std::printf("  [ ok ] %s\n", test.name);
      ++passed;
    } else {
      std::printf("  [FAIL] %s\n", test.name);
      for (size_t i = before; i < after; ++i) {
        std::printf("         %s\n", failures()[i].c_str());
      }
      ++failed;
    }
  }

  std::printf("\n----------------------------------------\n");
  std::printf("%d passed, %d failed, %d total\n", passed, failed,
              passed + failed);
  return failed == 0 ? 0 : 1;
}
