from pathlib import Path
import sys
a=Path(sys.argv[1]);out=Path(sys.argv[2]);s=a.read_text()
old='''        const unsigned cw=_mm_getcsr() & 0xffe0u;
        unsigned short x87cw; __asm__ volatile("fnstcw %0" : "=m"(x87cw));
        if ((cw==0x1fa0u || cw==0x9fe0u) && x87cw==0x037fu
            && endNodesValid && basis
            && bits(requestedHz)==bits(state.heldFrequencyHz)
            && bits(controls.pulseThresholdVolts)==bits(state.heldThresholdVolts)
            && controls.sawEnabled==state.sawEnabled && controls.pulseEnabled==state.pulseEnabled
            && (horizonRemaining!=0 || certifyHorizon())) {'''
new='''        // Integer-only exclusion precedes either FP-control read. It may
        // choose stock unnecessarily, never certifies a span by itself.
        if (bits(rate)==bits(96000.) && endNodesValid && basis
            && bits(requestedHz)==bits(state.heldFrequencyHz)
            && bits(controls.pulseThresholdVolts)==bits(state.heldThresholdVolts)
            && controls.sawEnabled==state.sawEnabled && controls.pulseEnabled==state.pulseEnabled) {
#if defined(CS80_WSC_HORIZON_OBSERVE)
            ++prefilterFpReads;
#endif
            unsigned rawCsr; unsigned short x87cw;
            __asm__ volatile("stmxcsr %0; fnstcw %1" : "=m"(rawCsr), "=m"(x87cw) :: "memory");
            const unsigned cw=rawCsr & 0xffe0u;
            if ((cw==0x1fa0u || cw==0x9fe0u) && x87cw==0x037fu
                && (horizonRemaining!=0 || certifyHorizon())) {'''
assert s.count(old)==1;s=s.replace(old,new)
old='            return output;\n        }\n#endif\n        horizonRemaining=0;';assert s.count(old)==1;s=s.replace(old,'            return output;\n          }\n        }\n#endif\n        horizonRemaining=0;')
old='    inline static std::uint64_t horizonFast=0,horizonSlow=0,horizonGrants=0;';assert s.count(old)==1;s=s.replace(old,old+'\n    inline static std::uint64_t prefilterFpReads=0;')
out.write_text(s)
