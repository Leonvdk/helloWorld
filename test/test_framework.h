// A very small test framework.
//
// Self-registering TEST() blocks, a handful of CHECK macros, and a main()
// that runs everything and prints a summary. No dependencies, so the suite
// builds with nothing but g++ on any machine.
#ifndef WINDCLOCK_TEST_FRAMEWORK_H
#define WINDCLOCK_TEST_FRAMEWORK_H

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace windclock_test {

struct TestCase {
  const char *suite;
  const char *name;
  void (*fn)();
};

// Constructed before main(), so every translation unit's tests are known
// by the time the runner starts.
inline std::vector<TestCase> &registry() {
  static std::vector<TestCase> tests;
  return tests;
}

inline std::vector<std::string> &failures() {
  static std::vector<std::string> messages;
  return messages;
}

struct Registrar {
  Registrar(const char *suite, const char *name, void (*fn)()) {
    registry().push_back(TestCase{suite, name, fn});
  }
};

inline void recordFailure(const char *file, int line, const std::string &what) {
  char buffer[1024];
  std::snprintf(buffer, sizeof(buffer), "%s:%d: %s", file, line, what.c_str());
  failures().push_back(buffer);
}

inline bool nearlyEqual(double a, double b, double tolerance) {
  if (std::isnan(a) || std::isnan(b)) return false;
  return std::fabs(a - b) <= tolerance;
}

inline std::string describe(double v) {
  char buffer[64];
  std::snprintf(buffer, sizeof(buffer), "%.6g", v);
  return buffer;
}

} // namespace windclock_test

#define TEST(suite_name, test_name)                                            \
  static void suite_name##_##test_name();                                      \
  static ::windclock_test::Registrar registrar_##suite_name##_##test_name(     \
      #suite_name, #test_name, &suite_name##_##test_name);                     \
  static void suite_name##_##test_name()

#define CHECK_TRUE(expr)                                                       \
  do {                                                                         \
    if (!(expr)) {                                                             \
      ::windclock_test::recordFailure(__FILE__, __LINE__,                      \
                                      "expected true: " #expr);                \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_FALSE(expr)                                                      \
  do {                                                                         \
    if ((expr)) {                                                              \
      ::windclock_test::recordFailure(__FILE__, __LINE__,                      \
                                      "expected false: " #expr);               \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_EQ(actual, expected)                                             \
  do {                                                                         \
    const auto check_actual_ = (actual);                                       \
    const auto check_expected_ = (expected);                                   \
    if (!(check_actual_ == check_expected_)) {                                 \
      ::windclock_test::recordFailure(                                         \
          __FILE__, __LINE__,                                                  \
          std::string(#actual " == " #expected " -- got ") +                   \
              ::windclock_test::describe(static_cast<double>(check_actual_)) +  \
              ", expected " +                                                  \
              ::windclock_test::describe(                                      \
                  static_cast<double>(check_expected_)));                      \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_NEAR(actual, expected, tolerance)                                \
  do {                                                                         \
    const double check_actual_ = static_cast<double>(actual);                  \
    const double check_expected_ = static_cast<double>(expected);              \
    if (!::windclock_test::nearlyEqual(check_actual_, check_expected_,         \
                                       (tolerance))) {                         \
      ::windclock_test::recordFailure(                                         \
          __FILE__, __LINE__,                                                  \
          std::string(#actual " ~= " #expected " -- got ") +                   \
              ::windclock_test::describe(check_actual_) + ", expected " +      \
              ::windclock_test::describe(check_expected_) + " +/- " +          \
              ::windclock_test::describe(static_cast<double>(tolerance)));     \
      return;                                                                  \
    }                                                                          \
  } while (0)

#define CHECK_STREQ(actual, expected)                                          \
  do {                                                                         \
    const std::string check_actual_((actual));                                 \
    const std::string check_expected_((expected));                             \
    if (check_actual_ != check_expected_) {                                    \
      ::windclock_test::recordFailure(__FILE__, __LINE__,                      \
                                      "got \"" + check_actual_ +               \
                                          "\", expected \"" +                  \
                                          check_expected_ + "\"");             \
    }                                                                          \
  } while (0)

#endif // WINDCLOCK_TEST_FRAMEWORK_H
