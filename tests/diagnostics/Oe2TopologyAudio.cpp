// Deterministic, unnormalized 96 kHz offline before/after examples.
// Existing modes compare byte-for-byte to frozen main. BOTH is a new controlled
// experimental comparator, not an existing main-session state.
#include "core/BbdChorus.h"
#include "FrozenBbdChorus.h"
#include <bit>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
int main(int argc,char** argv) {
    if(argc!=2) return 2;
    const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
    constexpr int rate=96000,frames=6*rate;
    constexpr double pi=3.14159265358979323846;
    int nonfinite=0,offMismatch=0;
    for(int mode=0;mode<4;++mode) {
        cs80::BbdChorus legacy,candidate;cs80_oe2_frozen::BbdChorus frozen;
        legacy.prepare(rate);candidate.prepare(rate);frozen.prepare(rate);
        std::ofstream oldFile(dir/(std::to_string(mode)+"-legacy.f32"),std::ios::binary);
        std::ofstream newFile(dir/(std::to_string(mode)+"-experimental.f32"),std::ios::binary);
        if(!oldFile||!newFile) return 2;
        for(int n=0;n<frames;++n) {
            const double t=double(n)/rate;
            float l=0,r=0;
            if(t<2) {
                const double fade=std::min(1.0,t/.025)*std::min(1.0,(2-t)/.025);
                l=r=float(fade*(.23*std::sin(2*pi*220*t)+.10*std::sin(2*pi*997*t)+.04*std::sin(2*pi*3020*t)));
            } else if(t<4) {
                const double pulse=std::fmod(t-2,.35),env=std::exp(-18*pulse)*(1-std::exp(-1000*pulse));
                l=float(.34*env*std::sin(2*pi*330*t));r=float(.17*env*std::sin(2*pi*551*t+.7));
            }
            const bool chorus=(mode&1)!=0,tremolo=(mode&2)!=0;
            const auto a=legacy.processExperimentalIndependent(l,r,4,.9f,chorus,tremolo,cs80::BbdChorus::ExperimentalTopology::historicalGrouping);
            const auto b=candidate.processExperimentalIndependent(l,r,4,.9f,chorus,tremolo,cs80::BbdChorus::ExperimentalTopology::sourceGraph);
            const float before[2]{a.l,a.r},after[2]{b.l,b.r};
            if(mode<3) {
                const auto f=frozen.process(l,r,4,.9f,chorus||tremolo,tremolo);
                if(std::bit_cast<std::uint32_t>(f.l)!=std::bit_cast<std::uint32_t>(a.l)||
                   std::bit_cast<std::uint32_t>(f.r)!=std::bit_cast<std::uint32_t>(a.r))return 1;
            }
            for(int k=0;k<2;++k) {
                if(!std::isfinite(before[k])||!std::isfinite(after[k])) ++nonfinite;
                if(mode==0&&std::bit_cast<std::uint32_t>(before[k])!=std::bit_cast<std::uint32_t>(after[k])) ++offMismatch;
            }
            oldFile.write(reinterpret_cast<const char*>(before),sizeof(before));
            newFile.write(reinterpret_cast<const char*>(after),sizeof(after));
        }
        if(!oldFile||!newFile) return 2;
    }
    std::printf("Audio pairs: four logical modes, 96000 Hz, six seconds each; nonfinite=%d off_mismatches=%d; no independent normalization; existing modes frozen-exact, BOTH new experiment\n",nonfinite,offMismatch);
    return nonfinite||offMismatch?1:0;
}
