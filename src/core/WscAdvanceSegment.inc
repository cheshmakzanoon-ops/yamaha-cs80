// Canonical physical advance; prepared terms belong to this same unshortened interval.
#if defined(CS80_WSC_REUSE_HELD_SPAN)
        const double e=prepared.decayFactor;
        const bool reset=state.resetRemainingSeconds>0.;
        const double pinSlope=prepared.pinSlope,uEnd=prepared.inputEnd;
        const double b=prepared.triangleAmplitude,a=start.triangle-b;
        state.triangleBaselineVolts=a+(state.triangleBaselineVolts-a)*e+b*prepared.secondsOverTau*e;
#else
        const double tau=profile.couplingSecondsNominal,e=decay(seconds/tau);
        const bool reset=state.resetRemainingSeconds>0.;
        const double pinSlope=reset?0.:-converterGain*state.heldFrequencyHz;
        const double uEnd=pinSlope*tau+(start.u-pinSlope*tau)*e;
        const double b=start.triangleGain*(start.u-pinSlope*tau),a=start.triangle-b;
        state.triangleBaselineVolts=a+(state.triangleBaselineVolts-a)*e+b*(seconds/tau)*e;
#endif
        state.charge+=(reset?1./profile.resetSecondsNominal-state.heldFrequencyHz:-state.heldFrequencyHz)*seconds;
        if(reset)state.resetRemainingSeconds=std::max(0.,state.resetRemainingSeconds-seconds);
        const double pinEnd=start.pin7+pinSlope*seconds;
        state.inputBaselineVolts=pinEnd-uEnd;
        if(std::abs(state.inputBaselineVolts)<1e-300)state.inputBaselineVolts=0.;
        if(std::abs(state.triangleBaselineVolts)<1e-300)state.triangleBaselineVolts=0.;
