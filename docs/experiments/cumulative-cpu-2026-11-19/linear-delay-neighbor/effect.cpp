// Isolated Linux/x86-64 effect replay. Compile this unchanged file against
// each checkout. Binary records are little-endian float words + MXCSR.
#include "core/Dsp.h"
#include <bit>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <immintrin.h>
#include <string_view>
#include <time.h>
#include <vector>

namespace {
struct Frame { float l, r; };
struct Plate {
    cs80::Reverb fx;
    void prepare(int rate) { fx.prepare(static_cast<float>(rate)); }
    void reset() { fx.reset(); }
    Frame process(Frame x, float, float) { return {fx.process(x.l, 0), fx.process(x.r, 1)}; }
};
struct Chorus {
    cs80::Chorus fx;
    void prepare(int rate) { fx.prepare(static_cast<float>(rate)); }
    void reset() { fx.reset(); }
    Frame process(Frame x, float a, float b) {
        const auto y = fx.process(x.l, x.r, 1.5f * a, b, true, false);
        return {y.l, y.r};
    }
};
std::int64_t threadNs() {
    timespec ts{};
    if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0) std::abort();
    return static_cast<std::int64_t>(ts.tv_sec) * 1000000000 + ts.tv_nsec;
}
unsigned number(std::string_view str, int base = 10) {
    unsigned out{};
    const auto result = std::from_chars(str.data(), str.data() + str.size(), out, base);
    if (result.ec != std::errc{} || result.ptr != str.data() + str.size()) std::exit(2);
    return out;
}

template<class Effect> void run(int frames, int rate, bool raw) {
    Effect effect, snapshot;
    effect.prepare(rate);
    std::vector<Frame> input(8192), output(raw ? 0 : frames);
    cs80::Rng rng(0x812a91u);
    for (auto& f : input) { f.l = 0.57f * rng.nextBipolar(); f.r = 0.67f * rng.nextBipolar(); }
    float a = 0.37f, b = 0.45f;
    const auto sample = [&](int i) {
        if ((i & 1023) == 0) {
            a = static_cast<float>((i / 1024) % 31) / 32.0f;
            b = static_cast<float>((i / 1024) % 29) / 31.0f;
        }
        return effect.process(input[static_cast<unsigned>(i) % input.size()], a, b);
    };
    for (int i = 0; i < 8192; ++i) (void)sample(i);
    if (raw) {
        for (int i = 0; i < frames; ++i) {
            if (i == frames/3) snapshot = effect;
            if (i == frames/2 || i == frames/2 + 1) effect.reset();
            if (i == 3*frames/5) effect = snapshot;
            if (i == 7*frames/10) effect.prepare(rate/2);
            _mm_setcsr(_mm_getcsr() & ~63u); // expose flags from this sample, not warmup
            const auto y = sample(i);
            const std::uint32_t record[] = {std::bit_cast<std::uint32_t>(y.l),
                                             std::bit_cast<std::uint32_t>(y.r), _mm_getcsr()};
            if (std::fwrite(record, sizeof record, 1, stdout) != 1) std::abort();
        }
        return;
    }
    const auto begin = threadNs();
    for (int i = 0; i < frames; ++i) output[i] = sample(i);
    const auto elapsed = threadNs() - begin;
    std::uint64_t hash = 14695981039346656037ULL;
    for (auto y : output) {
        hash = (hash ^ std::bit_cast<std::uint32_t>(y.l)) * 1099511628211ULL;
        hash = (hash ^ std::bit_cast<std::uint32_t>(y.r)) * 1099511628211ULL;
    }
    std::printf("%lld %016llx\n", static_cast<long long>(elapsed), static_cast<unsigned long long>(hash));
}
} // namespace

int main(int argc, char** argv) {
    if (argc != 6) return 2;
    const auto n = number(argv[2]), rate = number(argv[3]), control = number(argv[5], 16);
    const std::string_view scene(argv[1]), mode(argv[4]);
    if (n == 0 || n > 10000000 || rate < 16000 || rate > 1536000
        || (mode != "raw" && mode != "time")
        || (control != 0x1f80 && control != 0x9fc0 && control != 0x3f80 && control != 0x5f80 && control != 0x7f80))
        return 2;
    _mm_setcsr(control);
    if (scene == "plate") run<Plate>(n, rate, mode == "raw");
    else if (scene == "chorus") run<Chorus>(n, rate, mode == "raw");
    else return 2;
}
