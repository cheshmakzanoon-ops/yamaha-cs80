// Causal follow-up for the five retained release-floor assertion failures in
// a39-string-lifetime-probe.log. These are diagnostic control variants, not
// factory edits. The original whole-factory probe and its failures are retained.
#define main a39_lifetime_main
#include "a39-string-lifetime-probe.cpp"
#undef main

int main() {
    try {
        for (const char* name : {"Bowed Swell Strings", "Swell Strings", "Marcato Strings"}) {
            std::array<double, 2> idleLevel {}, releasedLevel {};
            for (int bypass = 0; bypass < 2; ++bypass) {
                FactoryPreset p = preset(name);
                if (bypass) p.values[P_chorusOn] = 0.0f;
                const std::string label = std::string(name) + (bypass ? " chorus-only bypass" : " complete factory");
                const auto played = render(p, {event(0.0, 1, 48, 0.80f), event(2.0, 0, 48)}, 6.0, label + " released");
                const auto idle = render(p, {}, 6.0, label + " never played");
                releasedLevel[static_cast<std::size_t>(bypass)] = played.rms(5.5, 5.95);
                idleLevel[static_cast<std::size_t>(bypass)] = idle.rms(5.5, 5.95);
                const double difference = db(releasedLevel[static_cast<std::size_t>(bypass)]
                                           / std::max(idleLevel[static_cast<std::size_t>(bypass)], 1e-15));
                check(std::abs(difference) < 0.25,
                      label + ": released output has settled within 0.25 dB of never-played idle floor");
                std::printf("FLOOR,%s,%d,%.9f,%.9f,%.9f,%d,%d\n", name, bypass,
                    db(releasedLevel[static_cast<std::size_t>(bypass)]),
                    db(idleLevel[static_cast<std::size_t>(bypass)]), difference,
                    played.finalActive, idle.finalActive);
            }
            const double reduction = db(idleLevel[1] / std::max(idleLevel[0], 1e-15));
            check(reduction < -12.0, std::string(name) + ": bypassing only chorus removes at least 12 dB of idle floor");
            std::printf("CHORUS_CAUSAL,%s,%.9f\n", name, reduction);
        }
    } catch (const std::exception& error) { check(false, error.what()); }
    std::printf("SUMMARY,checks=%d,failures=%d,renders=%d,stereo_frames=%lld,max_peak_dbfs=%.9f\n",
                checks, failures, renders, totalFrames, db(maximumPeak));
    return failures ? 1 : 0;
}
