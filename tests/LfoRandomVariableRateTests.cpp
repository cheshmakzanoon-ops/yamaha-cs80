// Ordinary finite-input standalone oscillator law, not a host or DSP-performance test.
#include "core/ModulationLfo.h"
#include "ModernLayerTolerances.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <stdexcept>

namespace {
constexpr long double kBoundaryClearance = 1.e-10L;
constexpr double kPiReference = 3.141592653589793238462643383279502884;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
// Independent integer recurrence and rational mapping; no production Rng calls.
struct ReferenceStream {
    std::uint32_t state;
    std::uint32_t next() {
        state = state ^ (state << 13);
        state = state ^ (state >> 17);
        state = state ^ (state << 5);
        return state;
    }
    float bipolar() { return float(static_cast<long double>(next() / 256u) / 8388608.L - 1.L); }
};
float inputRate(int n, int length) {
    constexpr double times[] {0., .21, .47, .73, 1.};
    constexpr double rates[] {16.37, 19.79, 17.13, 18.61, 16.89};
    const double t = double(n) / double(length - 1);
    int segment = 0;
    while (segment < 3 && t > times[segment + 1]) ++segment;
    const double alpha = (t - times[segment]) / (times[segment+1] - times[segment]);
    return float(rates[segment] + alpha * (rates[segment+1] - rates[segment]));
}
void capture(const std::filesystem::path& path, const std::vector<float>& values) {
    std::ofstream f(path, std::ios::binary);
    require(bool(f), "capture open");
    f.write(reinterpret_cast<const char*>(values.data()), std::streamsize(values.size()*sizeof(float)));
    require(bool(f), "complete capture");
}
}
int main(int argc, char** argv) {
    try {
        require(argc==2, "usage: random-variable-rate OUTPUT");
        ReferenceStream known{1u};
        for (auto expected : {270369u,67634689u,2647435461u,307599695u,2398689233u})
            require(known.next()==expected, "independent RNG known-answer vector");
        const std::filesystem::path out(argv[1]);
        require(!std::filesystem::exists(out), "fresh captures only");
        std::filesystem::create_directories(out);
        std::uint64_t total=0;
        int cases=0;
        std::cout << std::setprecision(15);
        for (int sr : {88200,96000,192000,384000}) {
            const int frames=sr/4;
            std::vector<float> rates(frames);
            for(int n=0;n<frames;++n) {
                rates[n]=inputRate(n,frames);
                require(std::isfinite(rates[n]) && rates[n]>=16.f && rates[n]<=20.f, "ordinary bounded input rate");
            }
            capture(out/("rates-"+std::to_string(sr)+".f32"),rates);
            for(int wave : {5,6}) {
                std::vector<float> previousSeed;
                for(std::uint32_t seed : {0x6c8e9cf5u,0x3b9e5d21u,0xa1c4f00du}) {
                    cs80::ModulationLfo actual(seed); actual.reset(0.f);
                    ReferenceStream reference{seed};
                    float from=reference.bipolar();
                    float to=wave==6?reference.bipolar():from;
                    long double cycles=0.L,minClearance=1.L;
                    unsigned previousCycle=0,wraps=0;
                    double worst=0.;
                    std::vector<float> observed,expected;observed.reserve(frames);expected.reserve(frames);
                    for(int n=0;n<frames;++n) {
                        // Integrate the exact float passed to production, independently at long-double precision.
                        cycles += static_cast<long double>(rates[n])/static_cast<long double>(sr);
                        const unsigned cycle=static_cast<unsigned>(std::floor(cycles));
                        const long double phase=cycles-static_cast<long double>(cycle);
                        minClearance=std::min(minClearance,std::min(phase,1.L-phase));
                        require(minClearance>kBoundaryClearance,"unambiguous reference wrap boundary; no skipped samples");
                        if(cycle!=previousCycle) {
                            require(cycle==previousCycle+1,"one ordinary wrap at a time");
                            ++wraps;previousCycle=cycle;
                            if(wave==5) from=reference.bipolar();
                            else {from=to;to=reference.bipolar();}
                        }
                        const double weight=(1.-std::cos(kPiReference*double(phase)))/2.;
                        const float truth=wave==5?from:float(double(from)+(double(to)-double(from))*weight);
                        const float value=actual.tick(rates[n],float(sr),wave);
                        require(std::isfinite(value)&&std::isfinite(truth)&&std::abs(value)<=1.f&&std::abs(truth)<=1.f,"finite bounded entire sequence");
                        if(wave==5) require(std::memcmp(&truth,&value,sizeof(float))==0,"exact held seeded sequence including wrap samples");
                        worst=std::max(worst,std::abs(double(value)-double(truth)));
                        observed.push_back(value);expected.push_back(truth);
                    }
                    require(wraps>=4,"at least four complete wraps");
                    require(worst<=cs80::tests::ws5::kLfoShapeAbsTolerance,"existing shape tolerance");
                    actual.reset(0.f);
                    for(int n=0;n<frames;++n) {
                        const float replay=actual.tick(rates[n],float(sr),wave);
                        require(std::memcmp(&replay,&observed[n],sizeof(float))==0,"reset repeats complete sequence exactly");
                    }
                    if(!previousSeed.empty()) require(previousSeed!=observed,"different engine seed yields different sequence");
                    previousSeed=observed;
                    const std::string id=std::to_string(sr)+"-wave"+std::to_string(wave)+"-seed"+std::to_string(seed);
                    capture(out/(id+"-actual.f32"),observed);capture(out/(id+"-reference.f32"),expected);
                    std::cout<<id<<" frames="<<frames<<" wraps="<<wraps<<" max_error="<<worst<<" min_boundary_clearance="<<double(minClearance)<<'\n';
                    ++cases;total+=frames;
                }
            }
        }
        std::cout<<"cases="<<cases<<" samples="<<total<<" reset_samples="<<total<<" sequence_captures="<<cases*2<<" rate_captures=4 failures=0\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
