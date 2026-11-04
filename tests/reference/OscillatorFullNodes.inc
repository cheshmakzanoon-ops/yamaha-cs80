// Frozen full node classifier from e4a4ee3dd9ff61f8520993442e0974be119605cd.
// A numerical continuation oracle, not independent hardware evidence.
// The production scheduler/state evolution is shared; this frozen classifier
// replaces the candidate affine fast path in a test-only generated class.
    Nodes nodes(int side) const noexcept {
        Nodes n;
        const double tau=profile.couplingSecondsNominal;
        const double pinSlope=state.resetRemainingSeconds>0.?0.:-converterGain*state.heldFrequencyHz;
        n.pin7=state.resetRemainingSeconds>0.?profile.resetVoltsNominal:converterGain*state.charge;
        n.u=n.pin7-state.inputBaselineVolts;n.uSlope=pinSlope-n.u/tau;
        const double fold=n.u-profile.sliceVoltsInferred;
        const double oriented=sided(fold,n.uSlope,side);
        const bool limited=sided(std::abs(fold)-rampHalf,(oriented>=0.?1.:-1.)*n.uSlope,side)>=0.;
        n.triangle=profile.triangleSpanVoltsNominal*std::min(1.,std::abs(fold)/rampHalf);
        n.triangleGain=limited?0.:std::copysign(profile.triangleSpanVoltsNominal/rampHalf,oriented);
        n.triangleSlope=n.triangleGain*n.uSlope;
        n.w=n.triangle-state.triangleBaselineVolts;n.wSlope=n.triangleSlope-n.w/tau;
        const double half=profile.triangleSpanVoltsNominal*.5,x=n.w/half;
        double a=std::abs(x);const double aSlope=std::copysign(1.,x)*n.wSlope/half;
        for(int k=1;k<=4;++k){const double knot=.25*double(k);if(std::abs(a-knot)<2e-11)a=knot+std::copysign(2e-11,aSlope*double(side));}
        double shapeSlope=0.;if(a<1.){const auto i=std::min(std::size_t(3),static_cast<std::size_t>(std::max(0.,a)*4.));shapeSlope=4.*(profile.shaperOrdinateInferred[i+1]-profile.shaperOrdinateInferred[i]);}
        const double comparator=sided(n.u-state.heldThresholdVolts,n.uSlope,side)>0.?profile.pulseHalfSpanVoltsInferred:-profile.pulseHalfSpanVoltsInferred;
        const double sine=-profile.sineGainVoltsInferred*shape(x,profile);
        n.ungatedValue={-n.u,comparator,sine};
        n.value={state.sawEnabled?-n.u:0.,state.pulseEnabled?comparator:0.,sine};
        n.slope={state.sawEnabled?-n.uSlope:0.,0.,-profile.sineGainVoltsInferred*shapeSlope*n.wSlope/half};
        n.constant={state.sawEnabled?-pinSlope*tau:0.,n.value.pulsePin7,n.value.sinePin2+profile.sineGainVoltsInferred*shapeSlope*x};
        return n;
    }
