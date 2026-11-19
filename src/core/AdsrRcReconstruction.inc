// Shared numerical body for the original RC entry and its Engine-only call site.
// Include inside the member after advancing preciseTime; not a standalone header.
        // Single-RC law; `log` selects the settle (decay/release) or attack constant.
        const auto coefficient = [this, sampleRate](float time, double log = kRcSettleLog) {
            return expD(-log / (std::max(time, 0.00005f) * static_cast<double>(sampleRate)));
        };
        switch (stage)
        {
            case Stage::Idle: preciseValue = 0.0; break;
            case Stage::Attack:
            {
                // Charge toward the overshoot target; the comparator ends at 1.
                const double target = 1.0 + (kRcAttackOvershoot - 1.0) * (1.0 - attackFrom);
                if (attackFrom < 1.0)
                    preciseValue = target + (preciseValue - target) * coefficient(a, kRcAttackLog);
                if (preciseValue >= 1.0 || attackFrom >= 1.0)
                {
                    preciseValue = 1.0; stage = Stage::Decay; preciseTime = 0.0;
                }
                break;
            }
            case Stage::Decay:
                preciseValue = s + (preciseValue - s) * coefficient(d);
                if (std::abs(preciseValue - s) < 0.0005f)
                {
                    preciseValue = s; stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain:
                if (liveSustain)
                {
                    preciseValue = s + (preciseValue - s) * coefficient(d);
                    if (std::abs(preciseValue - s) < 0.0005f)
                    {
                        preciseValue = s; liveSustain = false;
                    }
                }
                else preciseValue = s;
                break;
            case Stage::Release:
                preciseValue *= coefficient(r);
                if (preciseValue < 0.0002f) { preciseValue = 0.0; stage = Stage::Idle; }
                break;
        }
        stageTime = static_cast<float>(preciseTime);
        return value = static_cast<float>(preciseValue);
