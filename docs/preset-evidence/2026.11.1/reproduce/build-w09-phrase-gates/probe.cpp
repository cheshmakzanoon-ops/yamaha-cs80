#include "core/Engine.h"
#include "core/FactoryPresets.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

using namespace cs80;
constexpr int rate = 48000;
int checks = 0, failures = 0;
void check(bool condition, const char* what) {
    ++checks;
    if (!condition) { ++failures; std::fprintf(stderr, "FAIL: %s\n", what); }
}
const FactoryPreset& preset(const char* name) {
    const auto& bank = curatedLibrary();
    const auto found = std::find_if(bank.begin(), bank.end(), [&](const auto& p) { return p.name == name; });
    if (found == bank.end()) throw std::runtime_error(name);
    return *found;
}
std::unique_ptr<Engine> start(const FactoryPreset& p) {
    auto e = std::make_unique<Engine>();
    e->prepare(rate); e->setParams(p.values); e->noteOn(60, .8f, 1);
    check(e->getRenderPolicy() == RenderPolicy::sourcePerformance, "actual Circuit policy");
    return e;
}
struct Audio {
    double peak = 0, energy = 0;
    std::uint64_t samples = 0, nonfinite = 0, over = 0;
    std::uint64_t hash = 14695981039346656037ULL;
    void add(const float* a) {
        for (int ch=0; ch<2; ++ch) {
            ++samples;
            if (!std::isfinite(a[ch])) { ++nonfinite; continue; }
            const double s=a[ch]; peak=std::max(peak,std::abs(s)); energy += s*s;
            over += std::abs(s)>1.0;
            std::uint32_t bits; std::memcpy(&bits, a+ch, sizeof(bits));
            for (int i=0;i<4;++i) { hash ^= (bits >> (8*i)) & 255; hash *= 1099511628211ULL; }
        }
    }
    double peakDb() const { return 20*std::log10(std::max(peak,1e-15)); }
    double rmsDb() const { return 10*std::log10(std::max(energy/std::max<std::uint64_t>(samples,1),1e-30)); }
};
int active(const Engine& e) {
    int n=0; for(int i=0;i<8;++i) n+=e.getVoiceInfo(i).active; return n;
}
int holding(const Engine& e, bool pedal) {
    int n=0; for(int i=0;i<8;++i) {const auto v=e.getVoiceInfo(i);n+=pedal?v.pedalHeld:v.held;} return n;
}
void ring(FILE* results, FILE* points, FILE* audio) {
    constexpr const char* lanes[] = {"continuous", "overlap", "pedal", "released"};
    for (const char* name : {"UFO Landing", "Ring Sweep Bell"}) {
        std::array<std::unique_ptr<Engine>,4> e;
        std::array<Audio,4> m;
        for(auto& item:e) item=start(preset(name));
        double overlapEnv=0, overlapHz=0, pedalEnv=0, pedalHz=0, freshEnv=0, freshHz=0, freshDifference=0;
        bool pedalFirst=false, pedalBoth=false, afterPedal=false, freshHeld=false;
        for(int frame=0;frame<8*rate;++frame) {
            if(frame==19200) e[2]->controlChange(64,1,1); // 0.4s
            if(frame==24000) e[2]->noteOff(60,1);         // 0.5s
            if(frame==28800) {e[1]->noteOn(67,.8f,1);e[2]->noteOn(67,.8f,1);} //0.6s
            if(frame==43200) {e[1]->noteOff(67,1);e[2]->noteOff(67,1);}       //0.9s
            if(frame==48000) {e[1]->noteOff(60,1);e[2]->controlChange(64,0,1);e[3]->noteOff(60,1);} //1s
            if(frame==57600) for(int lane=1;lane<4;++lane) e[lane]->noteOn(72,.8f,1); //1.2s
            if(frame==76800) {e[0]->noteOff(60,1);for(int lane=1;lane<4;++lane)e[lane]->noteOff(72,1);} //1.6s
            for(int lane=0;lane<4;++lane) {float a[2];e[lane]->render(a,1);m[lane].add(a);}
            if(frame>=28800 && frame<48000) {
                overlapEnv=std::max(overlapEnv,std::abs(double(e[0]->getRingEnvelope())-e[1]->getRingEnvelope()));
                overlapHz=std::max(overlapHz,std::abs(double(e[0]->getRingFrequency())-e[1]->getRingFrequency()));
                pedalEnv=std::max(pedalEnv,std::abs(double(e[0]->getRingEnvelope())-e[2]->getRingEnvelope()));
                pedalHz=std::max(pedalHz,std::abs(double(e[0]->getRingFrequency())-e[2]->getRingFrequency()));
            }
            if(frame>=57600 && frame<76800) {
                freshEnv=std::max(freshEnv,std::abs(double(e[3]->getRingEnvelope())-e[2]->getRingEnvelope()));
                freshHz=std::max(freshHz,std::abs(double(e[3]->getRingFrequency())-e[2]->getRingFrequency()));
                freshDifference=std::max(freshDifference,std::abs(double(e[0]->getRingEnvelope())-e[2]->getRingEnvelope()));
            }
            if(frame==26400) pedalFirst=holding(*e[2],false)==0 && holding(*e[2],true)==1;
            if(frame==45600) pedalBoth=holding(*e[2],false)==0 && holding(*e[2],true)==2;
            if(frame==50400) afterPedal=holding(*e[2],false)==0 && holding(*e[2],true)==0;
            if(frame==60000) freshHeld=holding(*e[2],false)==1 && holding(*e[2],true)==0;
            if(frame==26400 || frame==31200 || frame==45600 || frame==50400 || frame==57599 || frame==57600 || frame==60000 || frame==62400 || frame==76799) {
                for(int lane=0;lane<4;++lane)
                    std::fprintf(points,"%s,%s,%.9f,%.12g,%.12g,%d,%d,%d\n",name,lanes[lane],double(frame+1)/rate,
                        e[lane]->getRingEnvelope(),e[lane]->getRingFrequency(),holding(*e[lane],false),holding(*e[lane],true),active(*e[lane]));
            }
        }
        check(overlapEnv==0 && overlapHz==0,"overlapping key continues shared ring history");
        check(pedalEnv==0 && pedalHz==0,"CC64-held notes continue shared ring history");
        check(freshEnv==0 && freshHz==0,"pedal release and new onset match direct new phrase");
        check(freshDifference>.1,"fresh phrase differs from uninterrupted old sweep");
        check(pedalFirst,"first note is pedal held after key-up");
        check(pedalBoth,"both notes remain pedal held before pedal release");
        check(afterPedal,"pedal release removes every held gate");
        check(freshHeld,"fresh attack opens a new key gate");
        std::fprintf(results,"%s,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%.12g,%d,%d,%d,%d\n",name,overlapEnv,overlapHz,pedalEnv,pedalHz,freshEnv,freshHz,freshDifference,pedalFirst,pedalBoth,afterPedal,freshHeld);
        for(int lane=0;lane<4;++lane) {
            check(m[lane].nonfinite==0 && m[lane].over==0,"ring phrase finite and below full scale");
            check(active(*e[lane])==0,"ring phrase voices retire");
            std::fprintf(audio,"ring,%s,%s,8,%llu,%.12g,%.12g,%llu,%llu,%d,%016llx\n",name,lanes[lane],(unsigned long long)m[lane].samples,m[lane].peakDb(),m[lane].rmsDb(),(unsigned long long)m[lane].nonfinite,(unsigned long long)m[lane].over,active(*e[lane]),(unsigned long long)m[lane].hash);
        }
    }
}
struct Pitch {
    double first=0,last=0,maxGridCents=0,maxStepCents=0,minMovingStepCents=1e100;
    std::uint64_t count=0, fractional=0,moving=0;
    std::set<int> levels;
    void add(double midi) {
        if(count==0) first=midi;
        else {const double step=100*std::abs(midi-last);maxStepCents=std::max(maxStepCents,step);if(step>.01){++moving;minMovingStepCents=std::min(minMovingStepCents,step);}}
        const double error=100*std::abs(midi-std::round(midi));
        maxGridCents=std::max(maxGridCents,error); fractional+=error>.01;
        levels.insert(int(std::lround(midi))); last=midi;++count;
    }
};
void glide(FILE* results, FILE* points, FILE* audio) {
    for(const char* name:{"Glide Lead","Glissando Lead"}) {
        const auto& p=preset(name);
        std::array<std::unique_ptr<Engine>,2> e;
        std::array<Audio,2> m;
        std::array<std::array<Pitch,2>,2> pitch;
        std::array<double,2> overlapError{};
        for(auto& item:e)item=start(p);
        for(int frame=0;frame<int(5.5*rate);++frame) {
            if(frame==21600)e[1]->noteOff(60,1); // separated at0.45s
            if(frame==24000)for(auto& item:e)item->noteOn(72,.8f,1);
            if(frame==36000)e[0]->noteOff(60,1); // overlap old note ends0.75s
            if(frame==108000)e[1]->noteOff(72,1); //2.25s
            if(frame==110400)for(auto& item:e)item->noteOn(55,.8f,1); //2.3s
            if(frame==122400)e[0]->noteOff(72,1); //2.55s
            if(frame==206400)for(auto& item:e)item->noteOff(55,1); //4.3s
            for(int lane=0;lane<2;++lane) {float a[2];e[lane]->render(a,1);m[lane].add(a);}
            const int interval=(frame>=24000 && frame<105600)?0:(frame>=110400 && frame<201600)?1:-1;
            if(interval<0)continue;
            const int wanted=interval==0?72:55;
            double notes[2]{};
            for(int lane=0;lane<2;++lane) {
                int slot=-1;
                for(int i=0;i<8;++i){const auto v=e[lane]->getVoiceInfo(i);if(v.held && v.note==wanted)slot=i;}
                if(slot<0)throw std::runtime_error("Expected current held glide note");
                const auto r=e[lane]->getRankInfo(slot,0);
                notes[lane]=69.0+12.0*std::log2(double(r.oscillatorHz)/double(p.values[P_tuningRef]));
                pitch[lane][interval].add(notes[lane]);
                const int offset=frame-(interval==0?24000:110400);
                if(offset==0 || offset==48 || offset==480 || offset==960 || offset==2400 || offset==4800 || offset==9600 || offset==19200 || offset==38400 || offset==57600 || offset==76800)
                    std::fprintf(points,"%s,%s,%d,%.9f,%.12g,%.12g\n",name,lane==0?"overlap":"separated",interval,double(offset+1)/rate,r.oscillatorHz,notes[lane]);
            }
            overlapError[interval]=std::max(overlapError[interval],100*std::abs(notes[0]-notes[1]));
        }
        const bool stepped=p.values[P_glideMode]==2;
        for(int lane=0;lane<2;++lane) {
            for(int interval=0;interval<2;++interval) {
                const auto& d=pitch[lane][interval];const int target=interval==0?72:55;
                check(d.count>1000,"glide has complete held-pitch history");
                check(std::abs(d.last-target)<.03,"glide approaches intended target within three cents");
                check(d.levels.size()>=10,"glide traverses multiple semitone regions");
                if(stepped) {
                    check(d.maxGridCents<.001,"glissando oscillator remains on semitone grid");
                    check(d.maxStepCents>99.99 && d.maxStepCents<100.01,"glissando transitions one semitone at a time");
                    check(d.fractional==0,"glissando has no materially fractional pitches");
                } else {
                    check(d.maxGridCents>49,"smooth glide passes between semitone grid points");
                    check(d.maxStepCents<.3,"smooth glide has sub-cent per-sample movement");
                    check(double(d.fractional)/d.count>.99,"smooth glide is predominantly fractional pitch");
                }
                std::fprintf(results,"%s,%s,%d,%d,%llu,%.12g,%.12g,%.12g,%.12g,%llu,%llu,%zu,%.12g\n",name,lane==0?"overlap":"separated",interval,target,(unsigned long long)d.count,d.first,d.last,d.maxGridCents,d.maxStepCents,(unsigned long long)d.fractional,(unsigned long long)d.moving,d.levels.size(),overlapError[interval]);
            }
            check(m[lane].nonfinite==0 && m[lane].over==0,"glide phrase finite and below full scale");
            check(active(*e[lane])==0,"glide phrase voices retire");
            std::fprintf(audio,"glide,%s,%s,5.5,%llu,%.12g,%.12g,%llu,%llu,%d,%016llx\n",name,lane==0?"overlap":"separated",(unsigned long long)m[lane].samples,m[lane].peakDb(),m[lane].rmsDb(),(unsigned long long)m[lane].nonfinite,(unsigned long long)m[lane].over,active(*e[lane]),(unsigned long long)m[lane].hash);
        }
    }
}
int main() {
    FILE* r=std::fopen("ring-results.csv","w");FILE* rp=std::fopen("ring-points.csv","w");
    FILE* g=std::fopen("glide-results.csv","w");FILE* gp=std::fopen("glide-points.csv","w");FILE* a=std::fopen("captures.csv","w");
    if(!r||!rp||!g||!gp||!a)throw std::runtime_error("Cannot open diagnostic outputs");
    std::fprintf(r,"program,overlap_env_error,overlap_hz_error,pedal_env_error,pedal_hz_error,fresh_env_error,fresh_hz_error,fresh_vs_continuous_env_difference,pedal_first,pedal_both,pedal_up,fresh_held\n");
    std::fprintf(rp,"program,lane,time_seconds,ring_envelope,ring_hz,held,pedal_held,active\n");
    std::fprintf(g,"program,lane,interval,target,samples,first_midi,last_midi,max_grid_error_cents,max_step_cents,fractional_samples,moving_steps,rounded_levels,overlap_vs_separated_max_cents\n");
    std::fprintf(gp,"program,lane,interval,since_onset_seconds,oscillator_hz,midi_pitch\n");
    std::fprintf(a,"kind,program,lane,seconds,samples,peak_dbfs,rms_dbfs,nonfinite,over_full_scale,final_active,audio_float32_le_fnv1a64\n");
    ring(r,rp,a);glide(g,gp,a);
    for(FILE* f:{r,rp,g,gp,a})std::fclose(f);
    std::printf("Phrase gates: %d checks, %d failures;12 captures,86 seconds; factory parameters unchanged.\n",checks,failures);
    return failures?1:0;
}
