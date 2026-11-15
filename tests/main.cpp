// HEM Dark Matter Series - test runner. Optional argument picks one plug-in group (pv, sr, ...).
#include "TestHarness.hpp"

#include <cstring>

void runPhantomVoxTests();
void runSyndicateTests();
void runApexTests();
void runIronLockTests();
void runRiderDarkMatterTests();

int main(int argc, char** argv) {
    auto want = [&](const char* k) { return argc < 2 || std::strcmp(argv[1], k) == 0; };
    if (want("pv")) runPhantomVoxTests();
    if (want("sr")) runSyndicateTests();
    if (want("ax")) runApexTests();
    if (want("il")) runIronLockTests();
    if (want("rd")) runRiderDarkMatterTests();
    std::printf("\n%d checks, %d failed\n", dmt::gChecks, dmt::gFails);
    return dmt::gFails == 0 ? 0 : 1;
}
