    if (settingsSmoothing.snapIsPending())
    {
        int slot = 0;
        visitSmoothedSettings(global, rankSettings, kNumRanks,
                              [&](float& field) { field = settingsSmoothing.snapToTarget(slot++); });
        settingsSmoothing.endSnap();
    }
    else if (!settingsSmoothing.settled())
    {
        int slot = 0;
        settingsSmoothing.beginAdvance();
        visitSmoothedSettings(global, rankSettings, kNumRanks,
                              [&](float& field) { field = settingsSmoothing.advance(slot++); });
        settingsSmoothing.endAdvance();
    }

    // The four contour levers are smoothed fields: a lever move reaches the
    // KBC held nodes (refreshed by the multiplexer, Phase 2.3) as they glide.
    if (global.kbdBrillianceLow != refreshedContourLevers[0] || global.kbdBrillianceHigh != refreshedContourLevers[1]
        || global.kbdLevelLow != refreshedContourLevers[2] || global.kbdLevelHigh != refreshedContourLevers[3])
        refreshKeyboardContour();

    // Controllers: keep the event-set targets, use smoothed values for this
    // sample only. A settled smoother returns its target bit for bit.
    auto& c = controllerSmoothing;
    const double k = controlSmoothing;
    const float ribbonTotal = ribbon + params[P_ribbon];
    c.bendTarget = globalBend;
    c.upperBendTarget = upperZoneBend;
    c.pitchWheelTarget = globalPitchWheel;
    c.modWheelTarget = modWheel;
    c.ribbonTarget = ribbon;
    c.channelPressureTarget = lastChannelPressure;
    if (c.snapPending)
    {
        c.bend.snap(globalBend);
        c.upperBend.snap(upperZoneBend);
        c.pitchWheel.snap(globalPitchWheel);
        c.modWheel.snap(modWheel);
        c.channelPressure.snap(lastChannelPressure);
        expressionOptics.reset(expression);
        c.ribbon.snap(ribbonTotal);
    }
// The masked path can omit the five exact +0 recurrences when all ten
// operands are raw positive zero. The finite positive smoothing coefficient
// makes the original updates exact and each controller snaps to its target.
#if defined(CS80_MASKED_ZERO_MASTER_SMOOTHING)
    const auto zeroTargets=std::bit_cast<std::uint32_t>(globalBend)
        | std::bit_cast<std::uint32_t>(upperZoneBend)
        | std::bit_cast<std::uint32_t>(globalPitchWheel)
        | std::bit_cast<std::uint32_t>(modWheel)
        | std::bit_cast<std::uint32_t>(lastChannelPressure);
    if (zeroTargets != 0 ||
       (std::bit_cast<std::uint64_t>(c.bend.value())
        | std::bit_cast<std::uint64_t>(c.upperBend.value())
        | std::bit_cast<std::uint64_t>(c.pitchWheel.value())
        | std::bit_cast<std::uint64_t>(c.modWheel.value())
        | std::bit_cast<std::uint64_t>(c.channelPressure.value())) != 0)
    {
        globalBend = (float) c.bend.advance(globalBend, k);
        // Each MPE master owns a separate zone, but both follow the same
        // controller dezipper. Keep the event target intact between samples.
        upperZoneBend = (float) c.upperBend.advance(upperZoneBend, k);
        globalPitchWheel = (float) c.pitchWheel.advance(globalPitchWheel, k);
        modWheel = (float) c.modWheel.advance(modWheel, k);
        // The shared matrix tap of channel pressure must follow the same
        // dezipper as its per-voice counterpart, not bypass it with event memory.
        lastChannelPressure = (float) c.channelPressure.advance(lastChannelPressure, k);
    }
#else
    globalBend = (float) c.bend.advance(globalBend, k);
    // Each MPE master owns a separate zone, but both follow the same
    // controller dezipper. Keep the event target intact between samples.
    upperZoneBend = (float) c.upperBend.advance(upperZoneBend, k);
    globalPitchWheel = (float) c.pitchWheel.advance(globalPitchWheel, k);
    modWheel = (float) c.modWheel.advance(modWheel, k);
    // The shared matrix tap of channel pressure must follow the same
    // dezipper as its per-voice counterpart, not bypass it with event memory.
    lastChannelPressure = (float) c.channelPressure.advance(lastChannelPressure, k);
#endif
    // The EXP volume path is an optical cell (ws4 hand-off, ExpressionOptics.h):
    // its declared log-R law is this controller's smoothing.
    expressionOptics.setPedal(expression);
    c.expressionGain = (float) expressionOptics.tick();
    const double ribbonSmoothed = c.ribbon.advance(ribbonTotal, k);
    if (!c.ribbon.atTarget(ribbonTotal))
        ribbon = (float) (ribbonSmoothed - params[P_ribbon]);
    for (int vi = 0; vi < kNumVoices; ++vi)
    {
        auto& v = voices[(size_t) vi];
        c.pressureTarget[(size_t) vi] = v.pressure;
        c.mpeBendTarget[(size_t) vi] = v.mpeBend;
        c.timbreTarget[(size_t) vi] = v.timbre;
        // A new note starts from its own touch, bend and timbre, not a glide
        // from the previous note on this circuit.
        if (c.snapPending || c.age[(size_t) vi] != v.age)
        {
            c.age[(size_t) vi] = v.age;
            c.pressure[(size_t) vi].snap(v.pressure);
            c.mpeBend[(size_t) vi].snap(v.mpeBend);
            c.timbre[(size_t) vi].snap(v.timbre);
        }
        // These three controls are often unused. Only raw positive zero in
        // all six inputs qualifies, never negative zero, a subnormal, or a
        // trajectory still returning to zero. The engine's k is finite normal.
        // Stock arithmetic then has exact zero operands/results throughout,
        // except for 1e-9 * 1 (also exact), and explicitly snaps to +0.
        const auto slot = static_cast<std::size_t>(vi);
        const auto targetBits = std::bit_cast<std::uint32_t>(v.pressure)
                              | std::bit_cast<std::uint32_t>(v.mpeBend)
                              | std::bit_cast<std::uint32_t>(v.timbre);
        if (targetBits == 0)
        {
            const auto stateBits = std::bit_cast<std::uint64_t>(c.pressure[slot].value())
                                 | std::bit_cast<std::uint64_t>(c.mpeBend[slot].value())
                                 | std::bit_cast<std::uint64_t>(c.timbre[slot].value());
            if (stateBits == 0)
                continue;
        }
        v.pressure = (float) c.pressure[(size_t) vi].advance(v.pressure, k);
        v.mpeBend = (float) c.mpeBend[(size_t) vi].advance(v.mpeBend, k);
        v.timbre = (float) c.timbre[(size_t) vi].advance(v.timbre, k);
    }
    c.snapPending = false;
