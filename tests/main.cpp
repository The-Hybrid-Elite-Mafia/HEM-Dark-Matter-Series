// HEM Dark Matter Series - test runner. Optional argument picks one plug-in group (pv, sr, ...).
#include "TestHarness.hpp"

#include <cstring>

void runPhantomVoxTests();
void runSyndicateTests();

int main(int argc, char** argv) {
    auto want = [&](const char* k) { return argc < 2 || std::strcmp(argv[1], k) == 0; };
    if (want("pv")) runPhantomVoxTests();
    if (want("sr")) runSyndicateTests();
    std::printf("\n%d checks, %d failed\n", dmt::gChecks, dmt::gFails);
    return dmt::gFails == 0 ? 0 : 1;
}
