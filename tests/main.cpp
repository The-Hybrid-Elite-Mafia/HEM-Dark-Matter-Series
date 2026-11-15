// HEM Dark Matter Series - test runner.
#include "TestHarness.hpp"

void runPhantomVoxTests();

int main() {
    runPhantomVoxTests();
    std::printf("\n%d checks, %d failed\n", dmt::gChecks, dmt::gFails);
    return dmt::gFails == 0 ? 0 : 1;
}
