// Shared numerical body for the original RC entry and its Engine-only call site.
// Include inside the member after advancing preciseTime; not a standalone header.
        // Single-RC law; `log` selects the settle (decay/release) or attack constant.
        const auto coefficient = [this, sampleRate](float time, double log = kRcSettleLog) {
            return expD(-log / (std::max(time, 0.00005f) * static_cast<double>(sampleRate)));
        };
        switch (stage)
        {
            case Stage::Idle:
                if (releaseSelected && liveRelease && preciseValue != il)
                {
                    preciseValue += (il - preciseValue) * (1.0 - coefficient(r));
                    if (std::abs(preciseValue - il) < 0.0008f) preciseValue = il;
                }
                break;
            case Stage::Attack:
            {
                // Charge IL -> AL toward an overshoot target; comparator at AL.
                const double span = static_cast<double>(al) - attackFrom;
                const double target = attackFrom + kRcAttackOvershoot * span;
                if (span != 0.0)
                    preciseValue = target + (preciseValue - target) * coefficient(a, kRcAttackLog);
                const bool reached = span > 0.0 ? preciseValue >= al
                                   : span < 0.0 ? preciseValue <= al : true;
                if (reached)
                {
                    preciseValue = al; stage = Stage::Decay; preciseTime = 0.0;
                }
                break;
            }
            case Stage::Decay:
                preciseValue = sustain + (preciseValue - sustain) * coefficient(d);
                if (std::abs(preciseValue - sustain) < 0.0008f)
                {
                    preciseValue = sustain; stage = Stage::Sustain;
                }
                break;
            case Stage::Sustain: preciseValue = sustain; break;
            case Stage::Release:
                preciseValue += (il - preciseValue) * (1.0 - coefficient(r));
                if (std::abs(preciseValue - il) < 0.0008f)
                {
                    preciseValue = il; stage = Stage::Idle;
                }
                break;
        }
        stageTime = static_cast<float>(preciseTime);
        return value = static_cast<float>(preciseValue);
