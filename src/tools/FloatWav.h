#pragma once
// Offline diagnostic export only. No signal processing or automatic level changes.
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <ostream>
#include <stdexcept>
#include <vector>

namespace cs80::tools {
inline void wavRequire(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
inline void validateWavGain(double gain) {
    wavRequire(std::isfinite(gain) && gain >= 0 && gain <= 1e6,
               "wav-gain must be finite and in [0,1000000]");
}
struct WavStats { double peak = 0; std::uint32_t aboveFullScale = 0; };
// Getter returns an unscaled endpoint. Multiply once in double, then round to float.
template<class Getter>
WavStats writeFloatWav(const std::filesystem::path& path, std::size_t count,
                       std::uint32_t rate, double gain, std::vector<std::filesystem::path>& owned, Getter sample) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    validateWavGain(gain);
    wavRequire(rate > 0 && rate <= std::numeric_limits<std::uint32_t>::max()/4,
               "WAV rate outside RIFF bounds");
    wavRequire(count > 0 && count <= (std::numeric_limits<std::uint32_t>::max()-50)/4,
               "WAV sample count outside RIFF bounds");
    auto scaled = [&](std::size_t i) {
        const double v = static_cast<double>(sample(i))*gain;
        wavRequire(std::isfinite(v) && std::abs(v) <= std::numeric_limits<float>::max(),
                   "WAV sample is nonfinite or outside float32 range");
        return static_cast<float>(v);
    };
    WavStats stats;
    for (std::size_t i=0; i<count; ++i) {
        const double v = scaled(i);
        stats.peak = std::max(stats.peak, std::abs(v));
        if (std::abs(v)>1) ++stats.aboveFullScale;
    }
    wavRequire(!std::filesystem::exists(std::filesystem::symlink_status(path)), "WAV already exists");
    std::ofstream f(path, std::ios::binary);
    wavRequire(bool(f), "Cannot create WAV");
    owned.push_back(path);
    f.exceptions(std::ios::badbit | std::ios::failbit);
    auto u16 = [&](std::uint16_t n) { for(int i=0;i<2;++i) f.put(static_cast<char>((n>>(i*8))&255)); };
    auto u32 = [&](std::uint32_t n) { for(int i=0;i<4;++i) f.put(static_cast<char>((n>>(i*8))&255)); };
    f.write("RIFF",4); u32(50+static_cast<std::uint32_t>(count)*4); f.write("WAVE",4);
    f.write("fmt ",4); u32(18); u16(3); u16(1); u32(rate); u32(rate*4); u16(4); u16(32); u16(0);
    f.write("fact",4); u32(4); u32(static_cast<std::uint32_t>(count));
    f.write("data",4); u32(static_cast<std::uint32_t>(count)*4);
    for(std::size_t i=0;i<count;++i) u32(std::bit_cast<std::uint32_t>(scaled(i)));
    f.flush(); f.close();
    return stats;
}
inline void wavStatsJson(std::ostream& f, const char* name, const WavStats& s) {
    f << "{\"file\":\"" << name << "\",\"float32_peak_abs\":" << s.peak
      << ",\"above_full_scale_samples\":" << s.aboveFullScale << '}';
}
inline void wavManifest(std::ostream& f, double gain, std::size_t count, double rate,
                        const char* signal, const WavStats& reference, const WavStats* realtime) {
    f << ",\n\"audio_export\":{\"format\":\"IEEE float32 mono little-endian WAV\",\"signal\":\"" << signal
      << "\",\"shared_gain\":" << gain << ",\"sample_count\":" << count << ",\"rate_hz\":" << rate
      << ",\"endpoint_span_s\":" << (count-1)/rate << ",\"playback_duration_s\":" << count/rate
      << ",\"clock\":\"N endpoint samples including t=0, sample i at i/rate\""
      << ",\"processing\":\"double endpoint times shared gain then float32 cast; no clipping, limiting, normalization, auto-level, latency shift or resampling\""
      << ",\"warning\":\"Start playback at low volume. Values above full scale retained; players may clip. Diagnostic only, not hardware or perceptual proof\",\"reference\":";
    wavStatsJson(f,"reference.wav",reference);
    if(realtime) { f << ",\"realtime\":"; wavStatsJson(f,"realtime.wav",*realtime); }
    f << '}';
}
} // namespace cs80::tools
