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
constexpr double kPhaseTolerance = 1.e-10;
constexpr double kPiReference = 3.141592653589793238462643383279502884;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
// Independent integer recurrence and rational mapping; no production Rng calls.
struct ReferenceStream {
    std::uint32_t state;
    unsigned draws=0;
    std::uint32_t next() {
        state = state ^ (state << 13);
        state = state ^ (state >> 17);
        state = state ^ (state << 5);
        ++draws;
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
        require(argc==2, "usage: random-switching OUTPUT");
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
            std::vector<int> wrapIndices;
            long double referenceCycles=0.L;
            unsigned lastCycle=0;
            for(int n=0;n<frames;++n) {
                referenceCycles+=static_cast<long double>(rates[n])/static_cast<long double>(sr);
                const unsigned cycle=static_cast<unsigned>(std::floor(referenceCycles));
                if(cycle!=lastCycle) { wrapIndices.push_back(n);lastCycle=cycle; }
            }
            require(wrapIndices.size()>=4,"reference schedule has four wraps");
            std::vector<int> switchIndices {int(.13*frames),wrapIndices[0],wrapIndices[0]+1,
                                           int(.61*frames),wrapIndices[2],int(.87*frames)};
            std::sort(switchIndices.begin(),switchIndices.end());
            require(switchIndices.front()>0 && switchIndices.back()<frames,"switch indices in range");
            require(std::adjacent_find(switchIndices.begin(),switchIndices.end())==switchIndices.end(),"six unique switches; no collision deduplication");
            require(std::count_if(switchIndices.begin(),switchIndices.end(),[&](int n){return std::binary_search(wrapIndices.begin(),wrapIndices.end(),n);})==2,"exactly two selection/wrap coincidences");
            for(int startingWave : {5,6}) {
                std::vector<float> waveInputs(frames);
                int selection=startingWave;
                for(int n=0;n<frames;++n) {
                    if(std::binary_search(switchIndices.begin(),switchIndices.end(),n)) selection=11-selection;
                    waveInputs[n]=float(selection); // New selection applies before this sample's tick.
                }
                capture(out/("waves-"+std::to_string(sr)+"-start"+std::to_string(startingWave)+".f32"),waveInputs);
                std::vector<float> previousSeed;
                for(std::uint32_t seed : {0x6c8e9cf5u,0x3b9e5d21u,0xa1c4f00du}) {
                    cs80::ModulationLfo actual(seed); actual.reset(0.f);
                    ReferenceStream reference{seed};
                    float from=0.f,to=0.f;
                    int previousWave=-1;
                    unsigned switches=0;
                    long double cycles=0.L,minClearance=1.L;
                    unsigned previousCycle=0,wraps=0;
                    double worst=0.,worstPhase=0.;
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
                        }
                        const int wave=int(waveInputs[n]);
                        const bool selectionChanged=wave!=previousWave;
                        const bool wrapped=std::binary_search(wrapIndices.begin(),wrapIndices.end(),n);
                        if(selectionChanged) {
                            if(previousWave!=-1) ++switches;
                            from=reference.bipolar();to=wave==6?reference.bipolar():from;
                            previousWave=wave;
                        } else if(wrapped) {
                            if(wave==5) from=reference.bipolar();
                            else {from=to;to=reference.bipolar();}
                        }
                        const double weight=(1.-std::cos(kPiReference*double(phase)))/2.;
                        const float truth=wave==5?from:float(double(from)+(double(to)-double(from))*weight);
                        const float value=actual.tick(rates[n],float(sr),wave);
                        const double observedPhase=actual.getShapePhase();
                        require(std::isfinite(observedPhase)&&observedPhase>=0.&&observedPhase<1.,"finite bounded observed phase");
                        worstPhase=std::max(worstPhase,std::abs(observedPhase-double(phase)));
                        require(worstPhase<=kPhaseTolerance,"observed phase stays continuous across selection");
                        require(std::isfinite(value)&&std::isfinite(truth)&&std::abs(value)<=1.f&&std::abs(truth)<=1.f,"finite bounded entire sequence");
                        if(wave==5) require(std::memcmp(&truth,&value,sizeof(float))==0,"exact held seeded sequence including wrap samples");
                        worst=std::max(worst,std::abs(double(value)-double(truth)));
                        observed.push_back(value);expected.push_back(truth);
                    }
                    require(wraps>=4,"at least four complete wraps");
                    require(switches==6,"six live switches");
                    require(reference.draws==(startingWave==5?12u:13u),"reference draw count includes selection-over-wrap priority");
                    require(worst<=cs80::tests::ws5::kLfoShapeAbsTolerance,"existing shape tolerance");
                    actual.reset(0.f);
                    for(int n=0;n<frames;++n) {
                        const float replay=actual.tick(rates[n],float(sr),int(waveInputs[n]));
                        require(std::memcmp(&replay,&observed[n],sizeof(float))==0,"reset repeats complete sequence exactly");
                    }
                    if(!previousSeed.empty()) require(previousSeed!=observed,"different engine seed yields different sequence");
                    previousSeed=observed;
                    const std::string id=std::to_string(sr)+"-start"+std::to_string(startingWave)+"-seed"+std::to_string(seed);
                    capture(out/(id+"-actual.f32"),observed);capture(out/(id+"-reference.f32"),expected);
                    std::cout<<id<<" frames="<<frames<<" wraps="<<wraps<<" switches="<<switches<<" draws="<<reference.draws<<" max_error="<<worst<<" max_phase_error="<<worstPhase<<" min_boundary_clearance="<<double(minClearance)<<'\n';
                    ++cases;total+=frames;
                }
            }
        }
        std::cout<<"cases="<<cases<<" samples="<<total<<" reset_samples="<<total<<" sequence_captures="<<cases*2<<" rate_captures=4 waveform_captures=8 failures=0\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
