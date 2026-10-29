// Host-side runner for the platform independent test suite.
// Build:  g++ -std=c++17 -O2 -Wall -Wextra -o /tmp/test_logic tests/test_logic.cpp src/core/*.cpp
#include <cstdio>
#include <string>

#include "../src/core/selftest_core.h"

int main() {
    const core::TestSummary s = core::run_core_selftests();
    std::fputs(s.log.c_str(), stdout);
    if (s.failed == 0) {
        std::printf("RESULT: PASS (%d checks)\n", s.passed);
        return 0;
    }
    std::printf("RESULT: FAIL (%d failed of %d)\n", s.failed, s.passed + s.failed);
    return 1;
}
