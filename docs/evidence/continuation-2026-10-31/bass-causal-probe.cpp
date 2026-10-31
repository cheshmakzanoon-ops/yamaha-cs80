#define main unusedAuditMain
#include "tools/engine_audit.cpp"
#undef main
namespace cs80_probe {
bool enabled=false;
long host=0;int sub=-1;
struct Tap {long host;int sub,rank;std::array<double,18> x;};
std::vector<Tap> taps;
void tap(int rank,const std::array<double,18>& row){if(rank==0)++sub;taps.push_back({host,sub,rank,row});}
}
int main(int argc,char** argv)
{
 try {
    require(argc==2,"one new output directory required");
    const fs::path directory=argv[1];require(!fs::exists(directory),"output already exists");fs::create_directories(directory);
    std::ofstream metrics(directory/"metrics.csv");metrics<<std::setprecision(14);
    metrics<<"rate,event_frame,peak_frame,jump,retrigger_score,split_render_max_abs\n";
    const auto programs=programSetups(true);auto setup=*std::find_if(programs.begin(),programs.end(),[](const Setup& s){return s.name=="Bass Stack";});
    setup.useProgrammer=false;
    for(int rate:{44100,48000,96000}) {
        const auto events=phrase();const long event=long(std::ceil(T::retrigOn2*rate/kBlock))*kBlock;
        const long count=long(std::llround((T::retrigOn2+.04)*rate));
        Engine engine;engine.prepare(rate);const ControlFrame frame{setup.values};require(engine.setControlFrame(frame),"frame rejected");engine.reset();
        Rendered traced;traced.rate=rate;traced.l.resize(count);traced.r.resize(count);
        std::vector<float> buffer(kBlock*2);size_t next=0;
        cs80_probe::taps.clear();cs80_probe::taps.reserve(20000);
        for(long at=0;at<count;at+=kBlock) {
            const int n=int(std::min(long(kBlock),count-at));
            while(next<events.size()&&long(std::ceil(events[next].t*rate/kBlock))*kBlock<=at) {
                const auto e=events[next++];if(e.on)engine.noteOn(e.note,e.vel);else engine.noteOff(e.note);
            }
            require(engine.setControlFrame(frame),"frame rejected");
            cs80_probe::enabled=at>=event-kBlock;
            if(cs80_probe::enabled) {
                for(int i=0;i<n;++i){cs80_probe::host=at+i;cs80_probe::sub=-1;engine.render(&buffer[size_t(i)*2],1);}
            }else engine.render(buffer.data(),n);
            for(int i=0;i<n;++i){traced.l[size_t(at+i)]=buffer[size_t(i)*2];traced.r[size_t(at+i)]=buffer[size_t(i)*2+1];}
        }
        cs80_probe::enabled=false;
        const auto unsplit=render(setup,rate,events,T::retrigOn2+.04);
        double splitDifference=0;
        for(size_t i=0;i<traced.l.size();++i)splitDifference=std::max({splitDifference,double(std::abs(traced.l[i]-unsplit.l[i])),double(std::abs(traced.r[i]-unsplit.r[i]))});
        require(splitDifference==0,"single-sample trace changes audio");
        const long end=event+long(std::llround(.008*rate));long peakFrame=event;double jump=0;
        for(long i=event;i<end;++i){const double value=std::max(std::abs(traced.l[i]-traced.l[i-1]),std::abs(traced.r[i]-traced.r[i-1]));if(value>jump){jump=value;peakFrame=i;}}
        const double reference=maxJump(traced,idx(traced,T::chordOn),idx(traced,T::chordOff));
        metrics<<rate<<','<<event<<','<<peakFrame<<','<<jump<<','<<jump/reference<<','<<splitDifference<<'\n';metrics.flush();
        std::ofstream binary(directory/("audio-"+std::to_string(rate)+".bin"),std::ios::binary);
        binary.write(reinterpret_cast<const char*>(traced.l.data()),std::streamsize(traced.l.size()*sizeof(float)));
        binary.write(reinterpret_cast<const char*>(traced.r.data()),std::streamsize(traced.r.size()*sizeof(float)));
        std::ofstream output(directory/("output-"+std::to_string(rate)+".csv"));output<<std::setprecision(14)<<"sample,left,right\n";
        for(long i=event-kBlock;i<count;++i)output<<i<<','<<traced.l[i]<<','<<traced.r[i]<<'\n';
        std::ofstream taps(directory/("rank-taps-"+std::to_string(rate)+".csv"));taps<<std::setprecision(14);
        taps<<"sample,internal_index,rank,oscillator_input,filtered,vca_input,amp,filter_eg,hp_cutoff,lp_cutoff,level_drive,li_node,coupling,rank_output,oscillator_hz,phase,ei_octaves,hp_bp,hp_lp,lp_bp,lp_lp\n";
        for(const auto& tap:cs80_probe::taps){taps<<tap.host<<','<<tap.sub<<','<<tap.rank;for(double x:tap.x)taps<<','<<x;taps<<'\n';}
        std::cout<<rate<<" score="<<jump/reference<<" peakOffset="<<peakFrame-event<<" split="<<splitDifference<<'\n';
    }
    return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
