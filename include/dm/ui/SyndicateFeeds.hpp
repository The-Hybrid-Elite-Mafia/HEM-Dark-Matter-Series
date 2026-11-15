#pragma once
// HEM Dark Matter Series - meter / display feed ids for the Syndicate Rack and its split plug-ins.
namespace dm::ui {
enum SrFeed : int {
    SrInL, SrInR, SrOutL, SrOutR,   // global meters (bottom bar)
    SrIn, SrOut,                    // mono-summed level for the column LEDs (dBFS)
    SrSat,                          // harmonic energy the SATURATE stage adds (dB)
    SrGr,                           // compressor gain reduction (dB, <= 0)
    SrTransient,                    // live transient envelope (dB)
    SrCorr,                         // stereo correlation (-1 .. +1)
    SrBandL, SrBandM, SrBandH,      // MAXIMIZE band activity (0 .. 100 %)
    SrLimiter,                      // output limiter gain reduction (dB, <= 0)
};
} // namespace dm::ui
