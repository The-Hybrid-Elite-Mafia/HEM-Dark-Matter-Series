#pragma once
// HEM Dark Matter Series - real FFT analyzer feed.
// Audio thread: push() into a lock-free single-producer ring (no allocation, no locks).
// UI thread:    compute() reads the latest N samples, windows them and runs a radix-2 FFT
//               into preallocated buffers, producing magnitude in dBFS per bin.
#include "dm/core/Math.hpp"

#include <atomic>
#include <complex>
#include <vector>

namespace dm {

class Fft {
public:
    void prepare(int size) {
        n_ = size;
        int bits = 0;
        while ((1 << bits) < n_) ++bits;
        rev_.assign(static_cast<std::size_t>(n_), 0);
        for (int i = 0; i < n_; ++i) {
            int r = 0;
            for (int b = 0; b < bits; ++b) r |= ((i >> b) & 1) << (bits - 1 - b);
            rev_[static_cast<std::size_t>(i)] = r;
        }
        tw_.resize(static_cast<std::size_t>(n_ / 2));
        for (int k = 0; k < n_ / 2; ++k)
            tw_[static_cast<std::size_t>(k)] = std::polar(1.0f, -kTwoPi * static_cast<float>(k) / static_cast<float>(n_));
    }
    [[nodiscard]] int size() const noexcept { return n_; }

    void forward(std::vector<std::complex<float>>& a) const noexcept {
        for (int i = 0; i < n_; ++i) {
            const int r = rev_[static_cast<std::size_t>(i)];
            if (r > i) std::swap(a[static_cast<std::size_t>(i)], a[static_cast<std::size_t>(r)]);
        }
        for (int len = 2; len <= n_; len <<= 1) {
            const int step = n_ / len;
            for (int i = 0; i < n_; i += len) {
                for (int j = 0; j < len / 2; ++j) {
                    const auto w = tw_[static_cast<std::size_t>(j * step)];
                    auto& u = a[static_cast<std::size_t>(i + j)];
                    auto& v = a[static_cast<std::size_t>(i + j + len / 2)];
                    const auto t = w * v;
                    v = u - t;
                    u = u + t;
                }
            }
        }
    }

private:
    int n_ = 0;
    std::vector<int> rev_;
    std::vector<std::complex<float>> tw_;
};

class AnalyzerTap {
public:
    static constexpr int kFftSize = 4096;
    static constexpr int kRing = 16384;

    void prepare(float sampleRate) {
        sr_ = sampleRate;
        ring_.assign(kRing, 0.0f);
        write_.store(0, std::memory_order_relaxed);
        fft_.prepare(kFftSize);
        work_.assign(kFftSize, {0.0f, 0.0f});
        win_.resize(kFftSize);
        float sum = 0;
        for (int i = 0; i < kFftSize; ++i) {
            win_[static_cast<std::size_t>(i)] = 0.5f - 0.5f * std::cos(kTwoPi * static_cast<float>(i) / (kFftSize - 1));
            sum += win_[static_cast<std::size_t>(i)];
        }
        winNorm_ = 2.0f / sum; // single-sided amplitude correction
        magDb_.assign(kFftSize / 2, -120.0f);
    }

    // Audio thread.
    void push(float l, float r) noexcept {
        const unsigned w = write_.load(std::memory_order_relaxed);
        ring_[w & (kRing - 1)] = 0.5f * (l + r);
        write_.store(w + 1, std::memory_order_release);
    }

    // UI thread. Returns magnitude in dBFS per bin (bin k = k * sampleRate / kFftSize Hz).
    const std::vector<float>& compute() {
        const unsigned end = write_.load(std::memory_order_acquire);
        for (int i = 0; i < kFftSize; ++i) {
            const unsigned idx = (end - kFftSize + static_cast<unsigned>(i)) & (kRing - 1);
            work_[static_cast<std::size_t>(i)] = {ring_[idx] * win_[static_cast<std::size_t>(i)], 0.0f};
        }
        fft_.forward(work_);
        for (int k = 0; k < kFftSize / 2; ++k)
            magDb_[static_cast<std::size_t>(k)] = gainToDb(std::abs(work_[static_cast<std::size_t>(k)]) * winNorm_);
        return magDb_;
    }
    [[nodiscard]] float binHz(int k) const noexcept { return static_cast<float>(k) * sr_ / kFftSize; }

private:
    float sr_ = 48000.0f, winNorm_ = 1.0f;
    std::vector<float> ring_;
    std::atomic<unsigned> write_{0};
    Fft fft_;
    std::vector<std::complex<float>> work_;
    std::vector<float> win_, magDb_;
};

} // namespace dm
