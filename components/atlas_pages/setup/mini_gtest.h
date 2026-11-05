// SPDX-License-Identifier: BSD-3-Clause
// Copyright 2026 The Atlas Authors.
//
// The few gtest macros the setup tests use, so the tests that need no engine run anywhere
// (atlas/tests/firstrun.sh). In the engine tree the real gtest is included instead.

#ifndef COMPONENTS_ATLAS_PAGES_SETUP_MINI_GTEST_H_
#define COMPONENTS_ATLAS_PAGES_SETUP_MINI_GTEST_H_

#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace mini_gtest {

struct Case {
  std::string name;
  std::function<void()> body;
};

inline std::vector<Case>& Cases() {
  static std::vector<Case> cases;
  return cases;
}

inline int& Failures() {
  static int failures = 0;
  return failures;
}

struct Register {
  Register(std::string name, std::function<void()> body) {
    Cases().push_back({std::move(name), std::move(body)});
  }
};

inline int RunAll() {
  for (const Case& c : Cases()) {
    const int before = Failures();
    c.body();
    std::cout << (Failures() == before ? "ok   " : "FAIL ") << c.name << "\n";
  }
  std::cout << Cases().size() << " tests, " << Failures() << " failed\n";
  return Failures() == 0 ? 0 : 1;
}

}  // namespace mini_gtest

#define TEST(suite, name)                                                    \
  static void suite##_##name();                                              \
  static mini_gtest::Register reg_##suite##_##name(#suite "." #name,         \
                                                   suite##_##name);          \
  static void suite##_##name()

#define MINI_CHECK(cond, text)                                              \
  do {                                                                       \
    if (!(cond)) {                                                           \
      ++mini_gtest::Failures();                                              \
      std::cout << __FILE__ << ":" << __LINE__ << ": " << text << "\n";      \
    }                                                                        \
  } while (0)

#define EXPECT_TRUE(c) MINI_CHECK((c), "expected true: " #c)
#define EXPECT_FALSE(c) MINI_CHECK(!(c), "expected false: " #c)
#define EXPECT_EQ(a, b) MINI_CHECK((a) == (b), "expected " #a " == " #b)

#endif  // COMPONENTS_ATLAS_PAGES_SETUP_MINI_GTEST_H_
