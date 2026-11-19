// Included in two functions: the ordinary public bus and the private
// already-certified equal-key specialization. Keep one estimate/Newton
// update law, and leave the unqualified scalar token stream intact.
    double top = 0.0;
#if defined(CS80_DETAIL_EQUAL_TRG5_DRIVES)
    top = a[0] * kFullScaleVolts;
#else
    for (double x : a) top = std::max(top, x * kFullScaleVolts);
#endif
    if (top <= 0.0) return 0.0;
    // Newton on an exponential climbs only ~nVt per step from far below, so
    // start from the single-diode drop estimate unless the guess is close.
    const double estimate = std::max(0.0, top - kDiodeEmissionThermal
        * std::log1p(top / kBusResistorOhms / kDiodeSaturationAmps));
    double v = std::abs(guess - estimate) < 0.1 ? std::clamp(guess, 0.0, top) : estimate;
    for (int i = 0; i < iterations; ++i)
    {
        double current = 0.0, slope = 0.0;
#if defined(CS80_DETAIL_EQUAL_TRG5_DRIVES)
        // All equal and finite, v clamped to [0, top]: no lane can meet the
        // ordinary skip threshold. Each term is shared only in THIS Newton
        // iteration; summing in lane order retains the original rounding.
        const double e = std::exp((top - v) / kDiodeEmissionThermal);
        const double ci = kDiodeSaturationAmps * (e - 1.0);
        const double si = kDiodeSaturationAmps * e / kDiodeEmissionThermal;
        for (std::size_t j = 0; j < N; ++j)
        {
            current += ci;
            slope += si;
        }
#else
        for (double x : a)
        {
            // A diode 0.9 V below conduction passes < 1e-17 A: skip it.
            if (x * kFullScaleVolts < v - 0.9) continue;
            const double e = std::exp((x * kFullScaleVolts - v) / kDiodeEmissionThermal);
            current += kDiodeSaturationAmps * (e - 1.0);
            slope += kDiodeSaturationAmps * e / kDiodeEmissionThermal;
        }
#endif
        const double f = current - v / kBusResistorOhms;
        const double step = f / (slope + 1.0 / kBusResistorOhms);
        v = std::clamp(v + step, 0.0, top);
        if (std::abs(step) < 1.0e-12) break;
    }
    return v;
