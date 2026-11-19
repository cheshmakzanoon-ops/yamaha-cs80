    // Research only. Four whole nominal ticks (never a time warp). All original
    // physical/capacitor updates, right endpoint and the residual readout still
    // execute sample by sample. Failed admission takes the complete stock tick.
    WscOutputVolts tick(double requestedHz,const WscControls& controls={}) noexcept {
#if defined(__GNUC__) && !defined(__clang__) && defined(__x86_64__) && defined(__SSE2__) \
    && !defined(__FAST_MATH__) && !defined(__FMA__) && !defined(__ASSOCIATIVE_MATH__) \
    && !defined(__RECIPROCAL_MATH__) && !defined(__NO_SIGNED_ZEROS__) \
    && (!defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__) \
    && (!defined(__FLT_EVAL_METHOD__) || __FLT_EVAL_METHOD__ == 0) \
    && !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER) && !defined(__NVCOMPILER) \
    && !defined(__PGI) && !defined(__ibmxl__) && !defined(_MSC_VER) && !defined(CS80_WSC_HORIZON_FORCE_OFF)
        const unsigned cw=_mm_getcsr() & 0xffe0u;
        unsigned short x87cw; __asm__ volatile("fnstcw %0" : "=m"(x87cw));
        if ((cw==0x1fa0u || cw==0x9fe0u) && x87cw==0x037fu
            && endNodesValid && basis
            && bits(requestedHz)==bits(state.heldFrequencyHz)
            && bits(controls.pulseThresholdVolts)==bits(state.heldThresholdVolts)
            && controls.sawEnabled==state.sawEnabled && controls.pulseEnabled==state.pulseEnabled
            && (horizonRemaining!=0 || certifyHorizon())) {
#if defined(CS80_WSC_HORIZON_VERIFY)
            bool branch=false;
            const double physical=std::max(0.,state.charge)/state.heldFrequencyHz;
            const double span=nextBranchEvent(endNodes,std::min(dt, physical),branch);
            if(branch || bits(span)!=bits(dt) || std::abs(dt-physical)<=2e-15) ++horizonFalseWitnesses;
#endif
            --horizonRemaining;
#if defined(CS80_WSC_HORIZON_OBSERVE)
            ++horizonFast;
#endif
            diagnostics.eventCount=0;
            const double inverseTauSamples=basis->inverseTauSamples,weightedArea=basis->weightedArea,weightedMoment=basis->weightedMoment;
            advance(dt,endNodes);
            auto after=nodes(1);
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
#endif
        horizonRemaining=0;
#if defined(CS80_WSC_HORIZON_OBSERVE)
        ++horizonSlow;
#endif
        return tickStock(requestedHz,controls);
    }
#if defined(CS80_WSC_HORIZON_OBSERVE)
    inline static std::uint64_t horizonFast=0,horizonSlow=0,horizonGrants=0;
#endif
#if defined(CS80_WSC_HORIZON_VERIFY)
    inline static std::uint64_t horizonFalseWitnesses=0;
#endif
private:
    static std::uint64_t bits(double v) noexcept { return std::bit_cast<std::uint64_t>(v); }
    static bool boundedOrdinary(double v,double limit) noexcept {
        const auto b=bits(v)&0x7fffffffffffffffull;
        return !b || (b>=bits(0x1p-80) && b<=bits(limit));
    }
    bool certifyHorizon() noexcept {
        // Every extra FP operand is bit-classified FIRST; no sNaN comparison
        // or denormal arithmetic on a decline. The added operations can raise
        // only a precision flag, which the admission predicate requires set.
        static constexpr OscillatorNominalProfile nominal{};
        if(bits(rate)!=bits(96000.) || bits(dt)!=bits(1./96000.)
           || std::memcmp(&profile,&nominal,sizeof(profile))!=0
           || bits(state.resetRemainingSeconds)!=bits(0.)
           || bits(state.heldFrequencyHz)<bits(16.) || bits(state.heldFrequencyHz)>bits(2048.)
           || bits(state.charge)<bits(0x1p-20) || bits(state.charge)>bits(1.)
           || !boundedOrdinary(state.heldThresholdVolts,4.)
           || !boundedOrdinary(state.inputBaselineVolts,8.)
           || !boundedOrdinary(state.triangleBaselineVolts,8.)
           || !boundedOrdinary(endNodes.u,8.) || !boundedOrdinary(endNodes.w,8.)
           || !boundedOrdinary(endNodes.triangleGain,3.)) return false;
        constexpr unsigned K=4;
        constexpr double invRate=1./96000.;
        const double f=state.heldFrequencyHz;
        // tau>.1, |u| and |w|<16, |pinSlope|<4f, |triangleGain|<3.
        // Four generous 1e-8 V error allowances dominate nominal dyadic
        // double cancellation in each physical update and event endpoint.
        const double du=K*(invRate*(4.*f+160.)+1.e-8);
        const double gain=std::abs(endNodes.triangleGain);
        const double Bmax=gain*(16.+4.*f*.104);
        const double dw=K*(0.00011*(16.+Bmax)+1.e-8);
        if(state.charge<=K*(f*invRate+1.e-10))return false;
#if !defined(CS80_WSC_HORIZON_BAD_BOUNDARY)
        if(std::abs(endNodes.u+rampHalf)<=du || std::abs(endNodes.u)<=du
           || std::abs(endNodes.u-rampHalf)<=du
           || std::abs(endNodes.u-state.heldThresholdVolts)<=du)return false;
        const double half=nominal.triangleSpanVoltsNominal*.5;
        for(int k=-4;k<=4;++k)if(k!=0 && std::abs(endNodes.w-(.25*half)*double(k))<=dw)return false;
        // At every covered start, B=triangleGain*(u-pinSlope*tau).
        // If |B| > 2(|w|+dw), then tau*(w+B)/B > tau/2 > 4dt.
        if(gain!=0.) {
            const double pinSlope=-converterGain*f,base=pinSlope*nominal.couplingSecondsNominal;
            const double B=endNodes.triangleGain*(endNodes.u-base);
            if(std::abs(B)<=2.*(std::abs(endNodes.w)+dw)+gain*du+1.e-8)return false;
        }
#endif
        horizonRemaining=K;
#if defined(CS80_WSC_HORIZON_OBSERVE)
        ++horizonGrants;
#endif
        return true;
    }
