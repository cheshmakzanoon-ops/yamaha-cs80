// Frozen combined branch search from 79dfade9925c2795b73fe27ceb580a3d8b8ab36d.
    double nextBranchEvent(const Nodes& n,double limit,bool& event) const noexcept {
        constexpr double epsilon=3e-14;
        const double tau=profile.couplingSecondsNominal;
        const double pinSlope=state.resetRemainingSeconds>0.?0.:-converterGain*state.heldFrequencyHz;
        const double base=pinSlope*tau,b=n.u-base;
        const double end=base+b*decay(limit/tau);
        const std::array<double,4> levels {profile.sliceVoltsInferred-rampHalf,profile.sliceVoltsInferred,profile.sliceVoltsInferred+rampHalf,state.heldThresholdVolts};
        for(double level:levels){if((level-n.u)*(level-end)<=0.&&level!=n.u&&b!=0.){const double root=-tau*std::log1p((level-n.u)/b);if(isFutureRoot(root,n.u-level,n.uSlope)&&root<=limit){limit=root;event=true;}}}
        const double triangleB=n.triangleGain*b;
        const auto w=[&](double t){return (n.w-triangleB*t/tau)*decay(t/tau);};
        // w has at most one turning point within an analytic fold segment.
        if(triangleB!=0.){const double turn=tau*(n.w+triangleB)/triangleB;if(turn>epsilon&&turn<limit){limit=turn;event=true;}}
        const double searchLimit=limit,wEnd=w(searchLimit),half=profile.triangleSpanVoltsNominal*.5;
        const bool increasing=wEnd>n.w;
        const double spacing=.25*half;
        int knot=increasing?int(std::floor((n.w+2e-11*half)/spacing))+1:int(std::ceil((n.w-2e-11*half)/spacing))-1;
        if(knot==0)knot=increasing?1:-1;
        if(increasing)knot=std::max(-4,knot);else knot=std::min(4,knot);
        if(knot>=-4&&knot<=4){const double level=spacing*double(knot);
            if((level-n.w)*(level-wEnd)<=0.&&level!=n.w){
                double lo=0.,hi=searchLimit,t=searchLimit*(level-n.w)/(wEnd-n.w);
                for(int iteration=0;iteration<5;++iteration){const double e=decay(t/tau),v=(n.w-triangleB*t/tau)*e,d=(-triangleB-n.w+triangleB*t/tau)*e/tau;
                    if((v<level)==increasing)lo=t;else hi=t;
                    const double candidate=d!=0.?t-(v-level)/d:(lo+hi)*.5;
                    if(std::abs(v-level)<1e-13&&std::abs(v-level)<=std::abs(d)*1e-11)break;
                    t=candidate>lo&&candidate<hi?candidate:(lo+hi)*.5;
                }
                // Tangential knees can have tiny voltage error but large time
                // error. Certify time error; rare shallow crossings get a
                // fixed-count long-double bisection, never an unbounded loop.
                const double ev=decay(t/tau),rv=(n.w-triangleB*t/tau)*ev-level,rd=(-triangleB-n.w+triangleB*t/tau)*ev/tau;
                if(std::abs(rd)<1e-3||std::abs(rv)>1e-13||std::abs(rv)>std::abs(rd)*1e-11){
                    long double lower=0.,upper=searchLimit;
                    for(int iteration=0;iteration<48;++iteration){const long double mid=(lower+upper)*.5L;
                        const long double value=(static_cast<long double>(n.w)-static_cast<long double>(triangleB)*mid/tau)*std::exp(-mid/tau);
                        if((value<level)==increasing)lower=mid;else upper=mid;}
                    t=static_cast<double>((lower+upper)*.5L);
                }
                if(isFutureRoot(t,(n.w-level)/half,n.wSlope/half)&&t<=limit){limit=t;event=true;}
            }
        }
        return limit;
    }
