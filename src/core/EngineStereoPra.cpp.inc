// The ordinary, masked GNU 14.2 stereo PRA solve; scalar fallback everywhere else.
// Not a horizontal mix: each lane retains its own four capacitor histories.
#if CS80_STEREO_PRA_BUILD
struct EngineStereoPra
{
    using W = PraWahCircuit;
    using V = double __attribute__((vector_size(16)));
    struct Out { V x,y; };
    // The integer energy proof and prefix/suffix comparison depend on the
    // private all-double representation. Fail closed on a future layout edit.
    static_assert(sizeof(W::Capacitor)==2*sizeof(double)
                  && offsetof(W::Capacitor,voltage)==0
                  && offsetof(W::Capacitor,current)==sizeof(double));
    static_assert(offsetof(W,profile)==0 && offsetof(W,caps)==sizeof(W::Profile)
                  && offsetof(W,g)==offsetof(W,caps)+sizeof(W::Capacitors)
                  && sizeof(W)==offsetof(W,targetLog)+sizeof(double));
    static inline bool boundedCaps(const W& l, const W& r) noexcept
    {
        // For magnitude >= 2^513, both top exponent bits (62,61) are 1.
        // Loads are integer-only and cannot quiet/drop a signaling operand.
        // Below this bound and with the fixed, positive factory network, every
        // intermediary is finite; the original scalar order owns all other states.
        auto bad=[](const W::Capacitor& c) {
            __m128i bits;std::memcpy(&bits,&c,sizeof bits);
            return _mm_and_si128(bits,_mm_add_epi64(bits,bits));
        };
        __m128i either=_mm_or_si128(bad(l.caps[0]),bad(r.caps[0]));
        for (int k=1;k<4;k++) either=_mm_or_si128(either,_mm_or_si128(bad(l.caps[k]),bad(r.caps[k])));
        return _mm_movemask_pd(_mm_castsi128_pd(_mm_add_epi64(either,either)))==0;
    }
    static __attribute__((always_inline)) inline bool common(const W& l, const W& r) noexcept
    {
        static_assert(std::is_standard_layout_v<W> && std::is_trivially_copyable_v<W>);
        const auto afterCaps = offsetof(W,g);
        const auto* a = reinterpret_cast<const unsigned char*>(&l);
        const auto* b = reinterpret_cast<const unsigned char*>(&r);
        // Compare all non-capacitor input values by representation. This also
        // rejects diverged optical states after a partial interrupted render.
        constexpr W::Profile factory{};
        if (std::memcmp(&l.profile,&factory,sizeof factory)!=0 || std::memcmp(&r.profile,&factory,sizeof factory)!=0
            || std::memcmp(a+afterCaps,b+afterCaps,sizeof(W)-afterCaps)!=0) return false;
        const auto small=[](double v,double hi) {return std::bit_cast<std::uint64_t>(v)<=std::bit_cast<std::uint64_t>(hi);};
        return small(l.g[0],32.) && small(l.g[1],32.) && small(l.g[2],32.) && small(l.g[3],32.)
            && small(l.heelLog,32.) && small(l.toeLog,32.) && small(l.opticalLog,32.) && small(l.targetLog,32.)
            && small(l.opticalStep,1.) && small(l.decayStep,1.);
    }
    static __attribute__((noinline)) Out run(W& left,W& right,float il,float ir) noexcept
    {
        const auto bounded=[](float x) {
            const double d=x;
            return std::isfinite(d) ? std::clamp(d,-W::maximumInputMagnitude,W::maximumInputMagnitude) : 0.;
        };
        const V u {bounded(il),bounded(ir)};
        auto& opticalLog=left.opticalLog;
        const double targetLog=left.targetLog;
        const auto& profile=left.profile;
        const auto& g=left.g;
        opticalLog+=(targetLog<opticalLog?left.opticalStep:left.decayStep)*(targetLog-opticalLog);
        right.opticalLog=opticalLog;
        const double optical=std::exp(opticalLog);
        const double rt=profile.vr1Ohms+optical/(1.0+optical/profile.parallelOhms);
        using Signal=V;
        struct PackedCap { Signal voltage,current; };
        std::array<PackedCap,4> caps;
        for (std::size_t k=0;k<caps.size();++k) {
            caps[k].voltage=V{left.caps[k].voltage,right.caps[k].voltage};
            caps[k].current=V{left.caps[k].current,right.caps[k].current};
        }
        struct { V x,n,a,p,b,q,c,d,y; } out;
#include "PraWahSolve.inc"
        for (std::size_t k=0;k<caps.size();++k) {
            left.caps[k]={caps[k].voltage[0],caps[k].current[0]};
            right.caps[k]={caps[k].voltage[1],caps[k].current[1]};
        }
        return {out.x,out.y};
    }
};
#endif
