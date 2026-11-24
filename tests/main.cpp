// HEM Dark Matter Series - test runner. Optional argument picks one plug-in group (pv, sr, ax, il, rd, cb, ui, plugin).
#include "TestHarness.hpp"

#include <cstring>

void runPhantomVoxTests();
void runSyndicateTests();
void runApexTests();
void runIronLockTests();
void runRiderDarkMatterTests();
void runChainbreakerTests();
void runFaceplateTests();
void runPluginTests();
void runQaReport();   // dm_tests qa: report only, never fails

int main(int argc, char** argv) {
    if (argc > 1 && std::strcmp(argv[1], "qa") == 0) { runQaReport(); return 0; }
    auto want = [&](const char* k) { return argc < 2 || std::strcmp(argv[1], k) == 0; };
    if (want("pv")) runPhantomVoxTests();
    if (want("sr")) runSyndicateTests();
    if (want("ax")) runApexTests();
    if (want("il")) runIronLockTests();
    if (want("rd")) runRiderDarkMatterTests();
    if (want("cb")) runChainbreakerTests();
    if (want("ui")) runFaceplateTests();
    if (want("plugin")) runPluginTests();
    std::printf("\n%d checks, %d failed\n", dmt::gChecks, dmt::gFails);
    return dmt::gFails == 0 ? 0 : 1;
}
