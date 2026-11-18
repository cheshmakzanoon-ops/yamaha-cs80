ControlFrame makeFrame(const Case& c, int selector) {
    ControlFrame frame;
    auto& p = frame.effectiveValues;
    p[P_parityMode] = c.isolated ? 1.0f : 0.0f;
    p[P_sourcePerformance] = c.sourcePerformance ? 1.0f : 0.0f;
    p[P_mix] = 0.5f;
    p[P_volume] = 0.12f; // fixed headroom, never per-tone normalization
    p[P_masterSustain] = p[P_keyRelease] = p[P_sustainMode] = 0.0f;
    p[P_chorusOn] = p[P_ringAmount] = 0.0f;
    p[P_subOriginalMode] = 1.0f;
    p[P_subVco] = p[P_subVcf] = p[P_subVca] = 0.0f;
    p[P_subAtVco] = p[P_subAtVcf] = p[P_subAtSpeed] = p[P_subExtLevel] = 0.0f;
    p[P_voiceDispersion] = p[P_drift] = 0.0f;
    p[P_analogNoise] = c.noise ? 0.12f : 0.0f;
    p[P_lfo1Depth] = p[P_lfo2Depth] = p[P_lfo3Depth] = 0.0f;
    p[P_fx1Type] = p[P_fx2Type] = p[P_fx3Type] = 0.0f;
    p[P_fx1Mix] = p[P_fx2Mix] = p[P_fx3Mix] = 0.0f;
    p[P_mpeOn] = p[P_arpOn] = 0.0f;
    p[P_footMode] = c.moving ? 1.0f : 0.0f;
    p[P_outputLevel] = p[P_outputMode] = 1.0f;
    if (c.performanceProfile != 0) {
        p[P_mpeOn] = 1.0f;
        p[P_mpeBendRange] = 12.0f;
        p[P_pitchBendRange] = 2.0f;
        p[P_footMode] = 1.0f;
        p[P_modWheelSub] = 0.15f;
        p[P_modWheelVcf] = 0.20f;
        if (c.performanceProfile == 1) {
            p[P_fx1Type] = 1.0f; p[P_fx1Mix] = 0.20f;
            p[P_fx2Type] = 4.0f; p[P_fx2Mix] = 0.15f;
        } else {
            p[P_fx1Type] = 2.0f; p[P_fx1Mix] = 0.20f;
            p[P_fx2Type] = 3.0f; p[P_fx2Mix] = 0.15f;
            p[P_fx3Type] = 5.0f; p[P_fx3Mix] = 0.10f;
        }
    }
    for (int rank = 0; rank < 2; ++rank) {
        frame.hardwareSources[rank] = selector;
        ParamValues legacy;
        applyHardwareTone(legacy, rank, selector);
        // Match the processor's physical selector adapter. Footage, pan and
        // software bypasses are not among the 26 replaced programmer controls.
        for (int local = 0; local < CP_count; ++local)
            if (local != CP_feet && local != CP_pan && local != CP_hpfOn && local != CP_lpfOn)
                p[chParam(rank, local)] = legacy[chParam(rank, local)];
        p[chParam(rank, CP_feet)] = 1.0f;
        p[chParam(rank, CP_pan)] = 0.0f;
        p[chParam(rank, CP_hpfOn)] = p[chParam(rank, CP_lpfOn)] = 1.0f;
    }
    return frame;
}
