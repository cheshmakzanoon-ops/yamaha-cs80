// A40: bounded follow-up to the observed -0.436 dBFS long-held Cathedral chord.
// Reuses the exact repository library audit for the phrase-floor measurement.
#define main a40_unused_library_audit_main
#include "tests/LibraryAuditTests.cpp"
#undef main
#include <memory>

namespace {
struct CathedralCase {
    const char* label;
    std::array<int, 8> notes;
    float velocity, pressure;
};
constexpr std::array<CathedralCase, 5> cathedralCases {{
    {"wide_full", {36,43,48,55,60,64,67,72}, 1.0f, 1.0f},
    {"close_middle_full", {48,50,52,53,55,57,59,60}, 1.0f, 1.0f},
    {"close_upper_full", {60,62,64,65,67,69,71,72}, 1.0f, 1.0f},
    {"wide_velocity_08", {36,43,48,55,60,64,67,72}, .8f, 1.0f},
    {"wide_pressure_zero", {36,43,48,55,60,64,67,72}, 1.0f, 0.0f},
}};
void cathedralRender(const FactoryPreset& preset, const CathedralCase& c, const char* trim) {
    constexpr int sampleRate = 48000, total = 12 * sampleRate;
    auto e = std::make_unique<Engine>(); e->prepare(sampleRate); e->setParams(preset.values);
    for (int n : c.notes) e->noteOn(n, c.velocity, 1);
    e->channelPressure(1, c.pressure);
    std::array<float, 512> block {};
    double peak = 0.0, sum = 0.0;
    long long nonfinite = 0, clipped = 0, lateCount = 0;
    int peakFrame = 0;
    for (int at = 0; at < total;) {
        const int count = std::min(256, total - at);
        e->render(block.data(), count);
        for (int i = 0; i < 2 * count; ++i) {
            const double x = block[static_cast<std::size_t>(i)];
            const int frame = at + i / 2;
            if (!std::isfinite(x)) { ++nonfinite; continue; }
            if (std::abs(x) >= 1.0) ++clipped;
            if (std::abs(x) > peak) { peak = std::abs(x); peakFrame = frame; }
            if (frame >= 10 * sampleRate) { sum += x*x; ++lateCount; }
        }
        at += count;
    }
    std::printf("CHORD,%s,%s,%.9f,%.2f,%.2f,12,%.9f,%.9f,%lld,%lld,%.9f\n",
        trim, c.label, preset.values[P_volume], c.velocity, c.pressure,
        dbfs(peak), dbfs(std::sqrt(sum/lateCount)), clipped, nonfinite, peakFrame / double(sampleRate));
    std::fflush(stdout);
}
}

int main() {
    FactoryPreset p;
    bool found = false;
    for (const auto& x : curatedLibrary()) if (x.name == "Cathedral Pad") { p = x; found = true; }
    if (!found) return 2;
    const float originalVolume = p.values[P_volume];
    const float proposedVolume = .685f;
    std::puts("kind,trim,scenario,volume,velocity,pressure,hold_seconds,peak_dbfs,late_rms_dbfs,clipped,nonfinite,peak_second");
    for (int trimmed = 0; trimmed < 2; ++trimmed) {
        p.values[P_volume] = trimmed ? proposedVolume : originalVolume;
        const char* label = trimmed ? "proposed_0685" : "source_6094";
        const auto a = measure(p);
        std::printf("AUDIT,%s,volume=%.9f,phrase_peak_dbfs=%.9f,window_rms_dbfs=%.9f,one_second_stress_dbfs=%.9f,finite=%d\n",
            label, p.values[P_volume], dbfs(a.phrasePeak), dbfs(a.windowRms), dbfs(a.stressPeak), a.finite ? 1 : 0);
        std::fflush(stdout);
        for (std::size_t i = 0; i < cathedralCases.size(); ++i) {
            if (trimmed && i >= 3) continue;
            cathedralRender(p, cathedralCases[i], label);
        }
    }
    std::fprintf(stderr, "A40 Cathedral bounded exploration complete: 8 twelve-second chord renders and 2 unchanged LibraryAuditTests::measure calls; no production data changed.\n");
    return 0;
}
