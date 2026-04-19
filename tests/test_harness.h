#pragma once

// Minimal zero-dep test harness. Not a replacement for a real framework, but
// enough for core-data-model tests without pulling Qt into non-UI code.

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace tuxels::testing {

struct Case {
  const char* name;
  void (*fn)();
};

inline std::vector<Case>& cases() {
  static std::vector<Case> v;
  return v;
}

struct Registrar {
  Registrar(const char* name, void (*fn)()) { cases().push_back({name, fn}); }
};

struct AssertionFailed {};

inline void failure(const char* file, int line, const std::string& msg) {
  std::fprintf(stderr, "  ASSERT FAIL at %s:%d — %s\n", file, line, msg.c_str());
  throw AssertionFailed{};
}

inline int run() {
  int passed = 0, failed = 0;
  for (const auto& c : cases()) {
    std::printf("[ RUN      ] %s\n", c.name);
    try {
      c.fn();
      std::printf("[       OK ] %s\n", c.name);
      ++passed;
    } catch (const AssertionFailed&) {
      std::printf("[   FAILED ] %s\n", c.name);
      ++failed;
    } catch (const std::exception& e) {
      std::fprintf(stderr, "  EXCEPTION: %s\n", e.what());
      std::printf("[   FAILED ] %s\n", c.name);
      ++failed;
    }
  }
  std::printf("\n%d passed, %d failed, %zu total\n", passed, failed, cases().size());
  return failed == 0 ? 0 : 1;
}

}  // namespace tuxels::testing

#define TEST(NAME)                                                \
  static void test_##NAME();                                      \
  static ::tuxels::testing::Registrar reg_##NAME(#NAME, test_##NAME); \
  static void test_##NAME()

#define CHECK(expr)                                                        \
  do {                                                                     \
    if (!(expr)) ::tuxels::testing::failure(__FILE__, __LINE__, #expr);    \
  } while (0)

#define CHECK_EQ(a, b)                                                     \
  do {                                                                     \
    auto _ta = (a);                                                        \
    auto _tb = (b);                                                        \
    if (!(_ta == _tb))                                                     \
      ::tuxels::testing::failure(__FILE__, __LINE__,                       \
                                 std::string(#a " == " #b " failed"));     \
  } while (0)

#define CHECK_APPROX(a, b, eps)                                            \
  do {                                                                     \
    if (!::tuxels::approxEqual((a), (b), (eps)))                           \
      ::tuxels::testing::failure(__FILE__, __LINE__,                       \
                                 std::string(#a " ~= " #b " failed"));     \
  } while (0)
