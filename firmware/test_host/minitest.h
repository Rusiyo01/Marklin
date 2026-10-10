// Mini framework de tests para compilar en el PC sin dependencias.
#pragma once

#include <stdio.h>

#include <functional>
#include <string>
#include <vector>

namespace minitest {

struct Case {
  const char* name;
  std::function<void()> fn;
};

inline std::vector<Case>& registry() {
  static std::vector<Case> r;
  return r;
}
inline int& failures() {
  static int f = 0;
  return f;
}

struct Registrar {
  Registrar(const char* name, std::function<void()> fn) { registry().push_back({name, fn}); }
};

inline int runAll() {
  int failedCases = 0;
  for (const auto& c : registry()) {
    const int before = failures();
    c.fn();
    const bool ok = failures() == before;
    if (!ok) failedCases++;
    printf("[%s] %s\n", ok ? " OK " : "FAIL", c.name);
  }
  printf("\n%zu tests, %d fallidos\n", registry().size(), failedCases);
  return failedCases == 0 ? 0 : 1;
}

}  // namespace minitest

#define MT_CONCAT2(a, b) a##b
#define MT_CONCAT(a, b) MT_CONCAT2(a, b)

#define TEST(name)                                                              \
  static void name();                                                           \
  static minitest::Registrar MT_CONCAT(reg_, name)(#name, name);                \
  static void name()

#define CHECK(cond)                                                             \
  do {                                                                          \
    if (!(cond)) {                                                              \
      printf("  %s:%d: fallo: %s\n", __FILE__, __LINE__, #cond);                \
      minitest::failures()++;                                                   \
    }                                                                           \
  } while (0)

#define CHECK_EQ(a, b)                                                          \
  do {                                                                          \
    const auto va_ = (a);                                                       \
    const auto vb_ = (b);                                                       \
    if (!(va_ == vb_)) {                                                        \
      printf("  %s:%d: fallo: %s == %s (%lld != %lld)\n", __FILE__, __LINE__,  \
             #a, #b, static_cast<long long>(va_), static_cast<long long>(vb_)); \
      minitest::failures()++;                                                   \
    }                                                                           \
  } while (0)

#define CHECK_STR(a, b)                                                         \
  do {                                                                          \
    const std::string va_ = (a);                                                \
    const std::string vb_ = (b);                                                \
    if (va_ != vb_) {                                                           \
      printf("  %s:%d: fallo: \"%s\" != \"%s\"\n", __FILE__, __LINE__,          \
             va_.c_str(), vb_.c_str());                                         \
      minitest::failures()++;                                                   \
    }                                                                           \
  } while (0)
