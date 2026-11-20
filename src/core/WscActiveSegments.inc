// Canonical active tick body. "after" is either an owned Nodes or a read-only view until the next right edge.
        double elapsed=0.;
        for(std::size_t iteration=0;iteration<maximumEventsPerTick&&elapsed<dt;++iteration){
            // A stopped q=0 capacitor does NOT retrigger the one-shot.
            if(state.resetRemainingSeconds==0.&&state.charge<=0.&&f>0.){
                before.set(after);state.charge=0.;state.resetRemainingSeconds=profile.resetSecondsNominal;++state.resetCount;
                after=nodes(1);recordEvent(elapsed,resetStart,before.value,after);
            }
            const bool inReset=state.resetRemainingSeconds>0.;
            const double physical=inReset?state.resetRemainingSeconds:(f>0.?std::max(0.,state.charge)/f:std::numeric_limits<double>::infinity());
            double span=std::min(dt-elapsed,physical);
            const auto start=after;
            bool branchEvent=false;
#if defined(CS80_WSC_REUSE_HELD_SPAN)
            const auto prepared=prepareSpan(start,span);
            span=nextHeldEvent(start,span,branchEvent,prepared);
#else
            span=nextBranchEvent(start,span,branchEvent);
#endif
            // Roots closer than round-off to the previous event are represented
            // by its right-sided branch derivative, never a zero-progress loop.
            if(!(span>0.)){diagnostics.eventCapacityExceeded=true;break;}
#if defined(CS80_WSC_REUSE_HELD_SPAN)
            // The search still visits every reset, fold, comparator, turn and
            // shaper knee. An event takes the original physical advance with
            // its returned duration, including an event exactly on the limit.
            if (branchEvent) advance(span,start);
            else advanceHeld(span,start,prepared);
#else
            advance(span,start);
#endif
            elapsed+=span;
            const bool physicalReached=std::abs(span-physical)<=2e-15;
            if(physicalReached&&inReset)state.resetRemainingSeconds=std::numeric_limits<double>::min();
            if(physicalReached||branchEvent)before.set(nodes(-1));
            if(physicalReached){
                if(inReset){state.resetRemainingSeconds=0.;after=nodes(1);recordEvent(elapsed,resetEnd,before.value,after);}
                else {state.charge=0.;state.resetRemainingSeconds=profile.resetSecondsNominal;++state.resetCount;after=nodes(1);recordEvent(elapsed,resetStart,before.value,after);}
            } else {after=nodes(1);if(branchEvent)recordEvent(elapsed,branchCorner,before.value,after);}
            if(dt-elapsed<=2e-15)elapsed=dt;
        }
        if(elapsed<dt){ // Explicit diagnostic, bounded numerical failure policy.
            diagnostics.eventCapacityExceeded=true;advance(dt-elapsed,after);after=nodes(1);
        }
        state.elapsedSeconds+=dt;
        endNodes=after;endNodesValid=true;
        const auto& raw=endNodes;
        diagnostics.rawPin7Volts=raw.pin7;diagnostics.coupledInputVolts=raw.u;
        diagnostics.trianglePin3Volts=raw.triangle;diagnostics.shaperInputVolts=raw.w;diagnostics.raw=raw.ungatedValue;
        WscOutputVolts output;
#if defined(CS80_WSC_BORROWED_EXPLICIT_TERMINAL)
        const auto finish=[&](double& pin, double value, double slope, double constant,
                              std::size_t branch) noexcept {
            const double a=value-constant,b=slope/rate+inverseTauSamples*a;
            pin=constant+a*weightedArea-b*weightedMoment+state.residual[branch][state.residualCursor];
            state.residual[branch][state.residualCursor]=0.;
        };
        finish(output.sawPin5,raw.value.sawPin5,raw.slope.sawPin5,raw.constant.sawPin5,0);
        // nodes() gives the held comparator an identical finite value and
        // constant, with exactly +0 slope. Its two basis coefficients are
        // therefore +0 in this masked-nearest path, and every omitted
        // operation is exact. The live residual (including gate transitions)
        // must still be added and consumed, even when the pulse is muted.
        output.pulsePin7=raw.value.pulsePin7+state.residual[1][state.residualCursor];
        state.residual[1][state.residualCursor]=0.;
        finish(output.sinePin2,raw.value.sinePin2,raw.slope.sinePin2,raw.constant.sinePin2,2);
#else
        double* destinations[3]={&output.sawPin5,&output.pulsePin7,&output.sinePin2};
        const auto values=asArray(raw.value),slopes=asArray(raw.slope),constants=asArray(raw.constant);
        for(std::size_t branch=0;branch<3;++branch){const double a=values[branch]-constants[branch],b=slopes[branch]/rate+inverseTauSamples*a;*destinations[branch]=constants[branch]+a*weightedArea-b*weightedMoment+state.residual[branch][state.residualCursor];state.residual[branch][state.residualCursor]=0.;}
#endif
        state.residualCursor=(state.residualCursor+1)%OscillatorResidualKernel::length;
        return output;
