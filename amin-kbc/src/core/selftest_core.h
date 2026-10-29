// =============================================================================
//  AMIN Keyboard & Clipboard - core/selftest_core.h
//  Platform independent test suite. The exact same code runs:
//    * on the build host  (tests/test_logic.cpp)
//    * on the user's PC   (AminKeyboardClipboard.exe --selftest)
// =============================================================================
#pragma once

#include <string>

namespace core {

struct TestSummary {
    int passed = 0;
    int failed = 0;
    std::string log;   // one line per test, failures first marked with [FAIL]
};

TestSummary run_core_selftests();

}  // namespace core
