#define main unusedAuditMain
#include "tools/engine_audit.cpp"
#undef main

namespace cs80_bass_probe {
bool capture=false,eventBoundary=false;
long host=0; int sub=-1,currentRate=0;
struct Tap {long host;int sub,rank;std::array<double,23> values;};
struct Snapshot {int rate;long sample;int voice,rank;std::string stage;std::vector<std::pair<std::string,double>> values;};
std::vector<Tap> taps;
std::vector<Snapshot> snapshots;
void tap(int rank,const std::array<double,23>& values){if(rank==0)++sub;taps.push_back({host,sub,rank,values});}
void assignment(const cs80::Engine& engine,int voice,const char* stage){
    for(int rank:{0,1}) {
        Snapshot result{currentRate,host,voice,rank,stage,{}};
        auto add=[&](const std::string& name,double value){result.values.emplace_back(name,value);};
        const auto osc=engine.getRankOscillatorState(voice,rank);
        add("osc_charge",osc.charge);add("osc_reset_remaining",osc.resetRemainingSeconds);
        add("osc_input_baseline",osc.inputBaselineVolts);add("osc_triangle_baseline",osc.triangleBaselineVolts);
        add("osc_held_frequency",osc.heldFrequencyHz);add("osc_held_threshold",osc.heldThresholdVolts);
        add("osc_elapsed",osc.elapsedSeconds);add("osc_saw_enabled",osc.sawEnabled);add("osc_pulse_enabled",osc.pulseEnabled);
        add("osc_residual_cursor",osc.residualCursor);add("osc_reset_count",osc.resetCount);
        for(size_t ch=0;ch<osc.residual.size();++ch)for(size_t i=0;i<osc.residual[ch].size();++i)
            add("osc_residual_"+std::to_string(ch)+"_"+std::to_string(i),osc.residual[ch][i]);
        const auto audio=engine.getRankAudioState(voice,rank);
        add("phase",audio.phase);
        add("hp_input",audio.hpCircuit.input);add("hp_feedback",audio.hpCircuit.feedback);add("hp_bp",audio.hpCircuit.bp);add("hp_lp",audio.hpCircuit.lp);
        add("lp_input",audio.lpCircuit.input);add("lp_feedback",audio.lpCircuit.feedback);add("lp_bp",audio.lpCircuit.bp);add("lp_lp",audio.lpCircuit.lp);
        const auto env=engine.getRankEnvelopeState(voice,rank);
        add("filter_envelope",env.filter.value);add("amplitude_envelope",env.amplitude.value);
        const auto vca=engine.getRankVcaState(voice,rank);add("vca_level",vca.level);add("vca_coupling",vca.coupling);
        snapshots.push_back(std::move(result));
    }
}
}
int main(int argc,char** argv){
try {
    require(argc==2,"one new output directory required");
    const fs::path dir=argv[1];require(!fs::exists(dir),"output directory must be new");fs::create_directories(dir);
    std::ofstream metrics(dir/"metrics.csv");metrics<<std::setprecision(17);
    metrics<<"rate,latency_samples,oversampling,event_frame,peak_frame,peak_delay_ms,peak_jump,retrigger_score,chord_reference,chord_peak_frame,first_strike_event,first_strike_peak_frame,first_strike_peak,first_strike_score,event_jump,omitted_score,prefix_max_abs,partition_max_abs\n";
    const auto programs=programSetups(true);auto setup=*std::find_if(programs.begin(),programs.end(),[](const Setup& s){return s.name=="Bass Stack";});setup.useProgrammer=false;
    const auto events=phrase();
    for(int rate:{48000,96000}){
        const long event=static_cast<long>(std::ceil(T::retrigOn2*rate/kBlock))*kBlock;
        const long count=static_cast<long>(std::llround((T::retrigOn2+.04)*rate));
        Engine engine;engine.prepare(rate);const ControlFrame frame{setup.values};require(engine.setControlFrame(frame),"physical frame rejected");engine.reset();
        Rendered trace;trace.rate=rate;trace.l.resize(count);trace.r.resize(count);
        std::array<float,kBlock*2> buffer{};size_t next=0;
        cs80_bass_probe::currentRate=rate;cs80_bass_probe::taps.clear();cs80_bass_probe::taps.reserve(40000);cs80_bass_probe::snapshots.clear();
        for(long at=0;at<count;at+=kBlock){
            const int n=static_cast<int>(std::min<long>(kBlock,count-at));
            cs80_bass_probe::capture=at>=event-kBlock;
            cs80_bass_probe::eventBoundary=at==event;
            cs80_bass_probe::host=at;
            while(next<events.size()&&static_cast<long>(std::ceil(events[next].t*rate/kBlock))*kBlock<=at){
                const auto e=events[next++];
                if(e.on){if(cs80_bass_probe::eventBoundary)cs80_bass_probe::assignment(engine,6,"before_pending_catchup");engine.noteOn(e.note,e.vel);}
                else engine.noteOff(e.note);
            }
            cs80_bass_probe::eventBoundary=false;
            require(engine.setControlFrame(frame),"physical frame rejected");
            if(cs80_bass_probe::capture)for(int i=0;i<n;++i){cs80_bass_probe::host=at+i;cs80_bass_probe::sub=-1;engine.render(buffer.data()+2*i,1);}
            else engine.render(buffer.data(),n);
            for(int i=0;i<n;++i){trace.l[at+i]=buffer[2*i];trace.r[at+i]=buffer[2*i+1];}
        }
        cs80_bass_probe::capture=cs80_bass_probe::eventBoundary=false;
        const auto unsplit=render(setup,rate,events,T::retrigOn2+.04);
        auto omittedEvents=events;
        omittedEvents.erase(std::remove_if(omittedEvents.begin(),omittedEvents.end(),[](const Event& e){return e.on&&e.t==T::retrigOn2&&e.note==60;}),omittedEvents.end());
        const auto omitted=render(setup,rate,omittedEvents,T::retrigOn2+.04);
        double split=0,prefix=0;for(long i=0;i<count;++i){split=std::max({split,std::abs(double(trace.l[i])-unsplit.l[i]),std::abs(double(trace.r[i])-unsplit.r[i])});if(i<event)prefix=std::max({prefix,std::abs(double(trace.l[i])-omitted.l[i]),std::abs(double(trace.r[i])-omitted.r[i])});}
        require(split==0,"single-sample tracing altered original block render");require(prefix==0,"omitted event changed pre-event audio");
        const long end=event+static_cast<long>(std::llround(.008*rate));long peak=event;double jump=0;
        for(long i=event;i<end;++i){const double value=std::max(std::abs(trace.l[i]-trace.l[i-1]),std::abs(trace.r[i]-trace.r[i-1]));if(value>jump){jump=value;peak=i;}}
        const double reference=maxJump(trace,idx(trace,T::chordOn),idx(trace,T::chordOff));
        const auto peakAt=[&](long start,long stop){long where=start;double largest=0;for(long i=std::max(1L,start);i<stop;++i){const double value=std::max(std::abs(trace.l[i]-trace.l[i-1]),std::abs(trace.r[i]-trace.r[i-1]));if(value>largest){largest=value;where=i;}}return where;};
        const long chordPeak=peakAt(idx(trace,T::chordOn),idx(trace,T::chordOff));
        const long firstEvent=static_cast<long>(std::ceil(T::retrigOn1*rate/kBlock))*kBlock;
        const long firstPeak=peakAt(firstEvent,firstEvent+static_cast<long>(std::llround(.008*rate)));
        const double firstJump=maxJump(trace,firstEvent,firstEvent+static_cast<long>(std::llround(.008*rate)));
        metrics<<rate<<','<<engine.getLatencySamples()<<','<<engine.getOversamplingFactor()<<','<<event<<','<<peak<<','<<1000.*(peak-event)/rate<<','<<jump<<','<<jump/reference<<','<<reference<<','<<chordPeak<<','<<firstEvent<<','<<firstPeak<<','<<firstJump<<','<<firstJump/reference<<','<<maxJump(trace,event,event+1)<<','<<maxJump(omitted,event,end)/reference<<','<<prefix<<','<<split<<'\n';metrics.flush();
        std::ofstream references(dir/("reference-windows-"+std::to_string(rate)+".csv"));references<<std::setprecision(17)<<"window,sample,left,right\n";
        const auto window=[&](const char* name,long first,long last){for(long i=first;i<last;++i)references<<name<<','<<i<<','<<trace.l[i]<<','<<trace.r[i]<<'\n';};
        window("chord_reference",chordPeak-128,chordPeak+129);
        window("first_strike",firstEvent-64,firstEvent+static_cast<long>(std::llround(.010*rate)));
        window("retrigger",event-64,event+static_cast<long>(std::llround(.010*rate)));
        std::ofstream audio(dir/("audio-"+std::to_string(rate)+".bin"),std::ios::binary);audio.write(reinterpret_cast<const char*>(trace.l.data()),trace.l.size()*sizeof(float));audio.write(reinterpret_cast<const char*>(trace.r.data()),trace.r.size()*sizeof(float));
        std::ofstream output(dir/("output-"+std::to_string(rate)+".csv"));output<<std::setprecision(17)<<"sample,left,right,omitted_left,omitted_right\n";
        for(long i=event-kBlock;i<count;++i)output<<i<<','<<trace.l[i]<<','<<trace.r[i]<<','<<omitted.l[i]<<','<<omitted.r[i]<<'\n';
        std::ofstream taps(dir/("rank-taps-"+std::to_string(rate)+".csv"));taps<<std::setprecision(17)<<"sample,internal_index,rank,oscillator_input,filtered,vca_input,amp,filter_eg,hp_cutoff,lp_cutoff,level_drive,li_node,coupling,rank_output,oscillator_hz,phase,ei_octaves,hp_bp,hp_lp,lp_bp,lp_lp,osc_charge,reset_remaining,reset_count,amp_stage,filter_stage\n";
        for(const auto& t:cs80_bass_probe::taps){taps<<t.host<<','<<t.sub<<','<<t.rank;for(const auto value:t.values)taps<<','<<value;taps<<'\n';}
        std::ofstream states(dir/("assignment-"+std::to_string(rate)+".csv"));states<<std::setprecision(17)<<"rate,sample,voice,rank,stage,field,value\n";
        for(const auto& snap:cs80_bass_probe::snapshots)for(const auto& [field,value]:snap.values)states<<snap.rate<<','<<snap.sample<<','<<snap.voice<<','<<snap.rank<<','<<snap.stage<<','<<field<<','<<value<<'\n';
        std::cout<<rate<<" score="<<jump/reference<<" eventDelta="<<maxJump(trace,event,event+1)<<" peak_ms="<<1000.*(peak-event)/rate<<" split="<<split<<'\n';
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
