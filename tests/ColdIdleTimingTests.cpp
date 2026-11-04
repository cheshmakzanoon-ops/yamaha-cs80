// Cold Engine scheduling against independent physical elapsed time and WSC charge.
// Registered in cold-idle-timing.json before candidate evaluation.
#include "core/Engine.h"
#include "Ws1Tolerances.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>
using namespace cs80;
namespace {
int checks=0,failures=0;
void check(bool ok,const char* text){++checks;if(!ok){++failures;std::printf("FAIL: %s\n",text);}}
// Independent phase-only restatement of the declared WSC capacitor law:
// q decreases at f; during reset it rises at (1/resetTime - f). The
// reset current folds one unit of charge back in over the one-shot interval.
// Shaper/coupling/antialias states cannot alter that physical charge trajectory.
struct ChargeTruth {
    double q=0,reset=0,frequency=0;
    explicit ChargeTruth(const ReconstructedOscillator::State& s):q(s.charge),reset(s.resetRemainingSeconds),frequency(s.heldFrequencyHz){}
    void tick(double f,double seconds){
        frequency=f;
        constexpr double resetTime=OscillatorNominalProfile{}.resetSecondsNominal;
        while(seconds>0){
            if(reset>0){const double span=std::min(seconds,reset);q+=(1/resetTime-f)*span;reset-=span;seconds-=span;continue;}
            if(q<=0){q=0;reset=resetTime;continue;}
            const double until=q/f;
            if(until<=seconds){q=0;reset=resetTime;seconds-=until;}
            else{q-=f*seconds;seconds=0;}
        }
    }
    double phase()const{return reset>0?frequency*(OscillatorNominalProfile{}.resetSecondsNominal-reset):1-q;}
};
double distance(double a,double b){double d=std::abs(a-b);d-=std::floor(d);return std::min(d,1-d);}
ParamValues fixture(bool pure){
    ParamValues p;p[P_sourcePerformance]=1;p[P_parityMode]=pure?1.f:0.f;p[P_oversampling]=1;
    p[P_voiceDispersion]=p[P_drift]=p[P_masterTune]=p[P_detune]=p[P_ribbon]=0;
    p[P_tuningRef]=440;p[P_portamento]=p[P_initBend]=0;
    p[P_subVco]=p[P_subAtVco]=p[P_subVcf]=p[P_subAtVcf]=p[P_subVca]=0;
    p[P_lfo1Depth]=p[P_lfo2Depth]=p[P_lfo3Depth]=p[P_modWheelSub]=0;
    p[P_chorusOn]=p[P_analogNoise]=p[P_ringAmount]=p[P_sustainMode]=0;
    for(int rank:{0,1}){
        p[chParam(rank,CP_feet)]=1;p[chParam(rank,CP_saw)]=1;p[chParam(rank,CP_pulse)]=0;
        p[chParam(rank,CP_pwmDepth)]=0;p[chParam(rank,CP_pw)]=.5f;
    }
    return p;
}
void run(const ws1::Tolerances& tol){
    double worstTime=0,worstPhase=0;
    for(double frequency:{250.,277.,2048.}){
        ReconstructedOscillator::State start;start.charge=.75;start.heldFrequencyHz=frequency;
        ChargeTruth oracle(start);
        for(int sample=0;sample<192000;++sample)oracle.tick(frequency,1./96000.);
        const double expected=std::fmod(.25+frequency*2.,1.);
        check(distance(oracle.phase(),expected)<=tol["oracle_constant_phase_error_cycles_max"],
              "phase-only truth matches analytic current-fold period");
    }
    for(int rate:{44100,48000,96000})for(bool pure:{false,true}){
        auto engine=std::make_unique<Engine>();engine->prepare(rate);
        const ControlFrame frame{fixture(pure)};check(engine->setControlFrame(frame),"cold timing fixture accepted");engine->reset();
        // Explicit reset must clear the priming marker after a prior run too.
        std::array<float,64> priorAudio{};engine->render(priorAudio.data(),32);engine->reset();
        const int factor=engine->getOversamplingFactor();const double fs=engine->getInternalSampleRate();
        std::vector<ChargeTruth> truth;std::array<double,16> began{};
        for(int voice=0;voice<8;++voice)for(int rank:{0,1}){
            const auto& state=engine->getRankOscillatorState(voice,rank);
            truth.emplace_back(state);began[size_t(voice*2+rank)]=state.elapsedSeconds;
        }
        std::vector<int> points{1,2,5,17,32,100,256,int(std::lround(.01*rate)),int(std::lround(.2*rate)),rate,int(std::lround(6.25*rate))};
        std::sort(points.begin(),points.end());points.erase(std::unique(points.begin(),points.end()),points.end());
        const float baseFrequency=440.f*std::pow(2.f,(61.f-69.f)/12.f);
        const float heldVolts=.5f*std::exp2((61.f-60.f)/12.f);
        int rendered=0;long internalSamples=0;double localTime=0,localPhase=0;
        std::array<float,512> audio{};
        for(int point:points){
            while(rendered<point){
                const int n=std::min(256,point-rendered);
                engine->render(audio.data(),n);rendered+=n;
                for(int sample=0;sample<n*factor;++sample,++internalSamples){
                    // SH7 holds on the first processed sample; subsequent samples
                    // follow its separately declared constant-current droop.
                    const double heldSeconds=double(internalSamples)/fs;
                    const float ratio=static_cast<float>(std::max(0.,double(heldVolts)-kas::kHoldLeakageAmps/kas::kHoldCapacitanceFarads*heldSeconds)/heldVolts);
                    const float frequency=baseFrequency*ratio;
                    for(auto& oscillator:truth)oscillator.tick(frequency,1/fs);
                }
            }
            // Engine owns SoA storage and non-owning rank views, so it is
            // deliberately noncopyable. Replay the identical deterministic
            // prehistory instead, including every render boundary and reset.
            // Assignment itself advances no sample and changes no prior control.
            auto waking=std::make_unique<Engine>();waking->prepare(rate);
            waking->setControlFrame(frame);waking->reset();
            waking->render(priorAudio.data(),32);waking->reset();
            int replayed=0;
            for(int boundary:points){
                if(boundary>point)break;
                while(replayed<boundary){const int n=std::min(256,boundary-replayed);waking->render(audio.data(),n);replayed+=n;}
            }
            for(int voice=0;voice<8;++voice)waking->noteOn(48+voice,.8f);
            for(int voice=0;voice<8;++voice)for(int rank:{0,1}){
                const int index=voice*2+rank;const auto& got=waking->getRankOscillatorState(voice,rank);
                const double expected=began[size_t(index)]+double(internalSamples)/fs;
                const double timeError=std::abs(got.elapsedSeconds-expected);
                const double phaseError=distance(waking->getRankAudioState(voice,rank).phase,truth[size_t(index)].phase());
                localTime=std::max(localTime,timeError);localPhase=std::max(localPhase,phaseError);
                check(timeError<=tol["elapsed_seconds_error_max"],"cold wake accounts only physically elapsed time");
                check(phaseError<=tol["phase_error_cycles_max"],"cold wake phase follows full-rate charge/SH7 law");
            }
        }
        worstTime=std::max(worstTime,localTime);worstPhase=std::max(worstPhase,localPhase);
        std::printf("cold idle rate=%d policy=%s worst_time=%.12g seconds worst_phase=%.12g cycles\n",rate,pure?"pure":"source",localTime,localPhase);
    }
    std::printf("cold idle worst elapsed error %.12g seconds; worst phase %.12g cycles\n",worstTime,worstPhase);
}
}
int main(int argc,char** argv){
    try{const ws1::Tolerances tol(argc>1?argv[1]:"tests/parity/tolerances-v2/cold-idle-timing.json","schema_version");run(tol);}
    catch(const std::exception& error){std::printf("ERROR: %s\n",error.what());return 2;}
    std::printf("Cold idle timing: %d checks, %d failures\n",checks,failures);return failures?1:0;
}
