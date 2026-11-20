// Reuse the archived differential observer, not its candidate or verdict.
// Each program links exactly one actual backend and emits a raw transcript.
#define main unusedStaticAdmissionMain
#include "tools/packed-coefficient-admission-research/component.cpp"
#undef main

int main(int argc,char** argv)
{
    if(argc!=2 || (file=std::fopen(argv[1],"wb"))==nullptr) return 2;
    const auto kernel=B::selectKernel();
    if(!kernel) return 3;
    std::fenv_t env; std::feholdexcept(&env);
    const auto saved=_mm_getcsr();
    const auto base=setup();
    const B::Inputs inputs{.125f,-.25f,.375f,-.5f};
    const std::array<double S::*,6> fields{&S::zIn,&S::zFb,&S::zBp,&S::zLp,&S::slope1,&S::slope2};
    const std::uint64_t words[]{0,1,0xfffffffffffff,0x0010000000000000,0x3fefffffffffffff,
                                0x3ff0000000000000,0x3ff0000000000001,0x3ff00000ffffffff,
                                0x408fffffffffffff,0x4090000000000000,0x4090000000000001,
                                0x40900000ffffffff,0x4090000100000000,0x7fefffffffffffff,
                                0x7ff0000000000000,0x7ff800000000001b,0x7ff0000000000029};
    for(unsigned rounding:{0u,0x2000u,0x4000u,0x6000u})
    for(unsigned denorm:{0u,0x40u,0x8000u,0x8040u})
    for(unsigned sticky:{0u,0x3fu})
    for(bool wrapper:{false,true})
    {
        const unsigned csr=0x1f80u|rounding|denorm|sticky;
        for(unsigned lane=0;lane<4;++lane) for(auto member:fields)
        for(auto magnitude:words) for(bool negative:{false,true})
        {
            auto stages=base;
            stages[lane].*member=std::bit_cast<double>(magnitude|(std::uint64_t(negative)<<63));
            run(stages,inputs,kernel,csr,wrapper);
        }
        // A failing magnitude must short-circuit the FP slope comparisons:
        // no denormal observation for the later rejected lane.
        for(unsigned lane=0;lane<4;++lane) for(auto z:{&S::zIn,&S::zFb,&S::zBp,&S::zLp})
        {
            auto stages=base;
            stages[lane].*z=std::bit_cast<double>(std::uint64_t{0x4090000000000001});
            stages[(lane+1)%4].slope1=std::bit_cast<double>(std::uint64_t{1});
            stages[(lane+2)%4].slope2=std::bit_cast<double>(std::uint64_t{0x8000000000000001});
            run(stages,inputs,kernel,csr,wrapper);
        }
    }
    std::fclose(file);
    std::fesetenv(&env);
    _mm_setcsr(saved);
    std::printf("PASS checks=%llu admitted=%llu rejected=%llu\n",
                (unsigned long long)checks,(unsigned long long)accepted,(unsigned long long)rejected);
    return accepted&&rejected ? 0:4;
}
