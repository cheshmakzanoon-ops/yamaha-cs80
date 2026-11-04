// Frozen combined scheduler from 79dfade9925c2795b73fe27ceb580a3d8b8ab36d.
    WscOutputVolts tick(double requestedHz,const WscControls& controls={}) noexcept {
        diagnostics.eventCount=0;
        if(!basis)return {}; // never prepared (basis pool exhausted at construction)
        const double inverseTauSamples=basis->inverseTauSamples,weightedArea=basis->weightedArea,weightedMoment=basis->weightedMoment;
        Nodes before;
        double f=state.heldFrequencyHz,threshold=state.heldThresholdVolts;
        if(std::isfinite(requestedHz)){f=std::clamp(requestedHz,0.,maximumFrequencyHz());if(f!=requestedHz)diagnostics.frequencyLimited=true;}
        else diagnostics.invalidControl=true;
        if(std::isfinite(controls.pulseThresholdVolts))threshold=controls.pulseThresholdVolts;
        else diagnostics.invalidControl=true;
        // A frequency or comparator-threshold change starts another analytic
        // segment even when its antialias residual is negligible. A cached
        // future crossing belongs to the OLD control law and must be discarded.
        if(f!=state.heldFrequencyHz||threshold!=state.heldThresholdVolts)
            branchEventCacheValid=false;
        // Control changes. A full event (branch residuals) is recorded unless the
        // change provably or negligibly alters no branch:
        // - exact: a threshold move that does not cross the comparator input
        //   leaves every branch unchanged; a frequency change during the reset
        //   plateau changes only the charge rate, which no branch reads;
        // - bounded: a frequency change whose slope corner adds less than
        //   kNegligibleCornerVolts (see below) keeps the new frequency but omits
        //   that corner's antialias residual.
        const bool gates=controls.sawEnabled!=state.sawEnabled||controls.pulseEnabled!=state.pulseEnabled;
        bool event=gates,refresh=gates||!endNodesValid;
        if(f!=state.heldFrequencyHz&&state.resetRemainingSeconds==0.){
            if(negligibleCornerScale*std::abs(f-state.heldFrequencyHz)<kNegligibleCornerVolts)refresh=true;
            else event=refresh=true;
        }
        if(threshold!=state.heldThresholdVolts&&!event){
            const double u=(state.resetRemainingSeconds>0.?profile.resetVoltsNominal:converterGain*state.charge)-state.inputBaselineVolts;
            const double a=u-state.heldThresholdVolts,b=u-threshold;
            if((a>0.)!=(b>0.)||std::abs(a)<1e-9||std::abs(b)<1e-9)event=refresh=true;
        }
        // Re-evaluate both sides of an actual control event with the same
        // classifier arithmetic. Affine continuation can differ by one ulp;
        // it must not invent a residual/event when a disabled gate is changed.
        if(event)before=nodes(1);
        state.heldFrequencyHz=f;state.heldThresholdVolts=threshold;state.sawEnabled=controls.sawEnabled;state.pulseEnabled=controls.pulseEnabled;
        auto after=refresh?nodes(1):endNodes;
        if(event)recordEvent(0.,controlChange,before,after);
        double elapsed=0.;
        for(std::size_t iteration=0;iteration<maximumEventsPerTick&&elapsed<dt;++iteration){
            // A stopped q=0 capacitor does NOT retrigger the one-shot.
            if(state.resetRemainingSeconds==0.&&state.charge<=0.&&f>0.){
                before=after;state.charge=0.;state.resetRemainingSeconds=profile.resetSecondsNominal;++state.resetCount;
                after=nodes(1);recordEvent(elapsed,resetStart,before,after);
                branchEventCacheValid=false;
            }
            const bool inReset=state.resetRemainingSeconds>0.;
            const double physical=inReset?state.resetRemainingSeconds:(f>0.?std::max(0.,state.charge)/f:std::numeric_limits<double>::infinity());
            double span=std::min(dt-elapsed,physical);
            const auto start=after;
            bool branchEvent=false;
            // The branch equations remain analytic across sample boundaries.
            // Search to the next physical boundary ONCE, then count down the
            // exact crossing time. A stopped oscillator has no finite physical
            // boundary: retain the bounded one-sample search in that case.
            if(!branchEventCacheValid){
                cachedBranchEvent=false;
                const double horizon=std::isfinite(physical)?physical:span;
                cachedBranchSeconds=nextBranchEvent(start,horizon,cachedBranchEvent);
                branchEventCacheValid=std::isfinite(physical);
                ++diagnostics.branchSearchCount;
            }
            if(cachedBranchEvent&&cachedBranchSeconds<=span){span=cachedBranchSeconds;branchEvent=true;}
            // Roots closer than round-off to the previous event are represented
            // by its right-sided branch derivative, never a zero-progress loop.
            if(!(span>0.)){diagnostics.eventCapacityExceeded=true;break;}
            advance(span,start);
            elapsed+=span;
            if(branchEventCacheValid)cachedBranchSeconds=std::max(0.,cachedBranchSeconds-span);
            const bool physicalReached=std::abs(span-physical)<=2e-15;
            if(physicalReached||branchEvent)branchEventCacheValid=false;
            if(physicalReached&&inReset)state.resetRemainingSeconds=std::numeric_limits<double>::min();
            if(physicalReached||branchEvent)before=nodes(-1);
            if(physicalReached){
                if(inReset){state.resetRemainingSeconds=0.;after=nodes(1);recordEvent(elapsed,resetEnd,before,after);}
                else {state.charge=0.;state.resetRemainingSeconds=profile.resetSecondsNominal;++state.resetCount;after=nodes(1);recordEvent(elapsed,resetStart,before,after);}
            } else if(branchEvent){after=nodes(1);recordEvent(elapsed,branchCorner,before,after);}
            else after=continuationNodes(start);
            if(dt-elapsed<=2e-15)elapsed=dt;
        }
        if(elapsed<dt){ // Explicit diagnostic, bounded numerical failure policy.
            diagnostics.eventCapacityExceeded=true;advance(dt-elapsed,after);after=nodes(1);branchEventCacheValid=false;
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
    }
