#pragma once
// HEM Dark Matter Series - plug-in-specific meter / display feed ids. Ids 0..15 are the shared
// EngineShell ones in SyndicateFeeds.hpp (bottom bar, column levels, limiter).
namespace dm::ui {
enum ApexFeed : int { AxGr = 100, AxWidth };
enum IronFeed : int { IlCorrection = 200, IlPitch };
enum RiderFeed : int { RdGain = 300 };
enum DmFeed : int { DmDuckL = 400, DmDuckR, DmReverb, DmDelay };
enum CbFeed : int { CbClipL = 500, CbClipM, CbClipH, CbStutHz };
} // namespace dm::ui
