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
            span=nextBranchEvent(start,span,branchEvent);
            // Roots closer than round-off to the previous event are represented
            // by its right-sided branch derivative, never a zero-progress loop.
            if(!(span>0.)){diagnostics.eventCapacityExceeded=true;break;}
            advance(span,start);
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
        double* destinations[3]={&output.sawPin5,&output.pulsePin7,&output.sinePin2};
        const auto values=asArray(raw.value),slopes=asArray(raw.slope),constants=asArray(raw.constant);
        for(std::size_t branch=0;branch<3;++branch){const double a=values[branch]-constants[branch],b=slopes[branch]/rate+inverseTauSamples*a;*destinations[branch]=constants[branch]+a*weightedArea-b*weightedMoment+state.residual[branch][state.residualCursor];state.residual[branch][state.residualCursor]=0.;}
        state.residualCursor=(state.residualCursor+1)%OscillatorResidualKernel::length;
        return output;
