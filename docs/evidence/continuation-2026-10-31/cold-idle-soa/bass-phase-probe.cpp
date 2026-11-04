#define main unusedAuditMain
#include "tools/engine_audit.cpp"
#undef main
int main(int argc,char** argv){
    require(argc==2,"one new prefix output directory required");
    const fs::path dir=argv[1];require(!fs::exists(dir),"output directory already exists");fs::create_directories(dir);
    std::cout<<std::setprecision(14)<<"rate,rank,phase,elapsed_seconds\n";
    const auto programs=programSetups(true);
    const auto setup=*std::find_if(programs.begin(),programs.end(),[](const Setup& s){return s.name=="Bass Stack";});
    const auto events=phrase();
    for(int rate:{44100,48000,96000}){
        Engine engine;engine.prepare(rate);const ControlFrame frame{setup.values};require(engine.setControlFrame(frame),"frame rejected");engine.reset();
        const long event=long(std::ceil(T::retrigOn2*rate/kBlock))*kBlock;
        const long stop=event+1;
        std::ofstream prefix(dir/(std::to_string(rate)+".bin"),std::ios::binary);
        size_t next=0;std::array<float,kBlock*2> audio{};
        for(long at=0;at<stop;at+=kBlock){
            while(next<events.size()&&long(std::ceil(events[next].t*rate/kBlock))*kBlock<=at){
                const auto event=events[next++];if(event.on)engine.noteOn(event.note,event.vel);else engine.noteOff(event.note);
            }
            require(engine.setControlFrame(frame),"frame rejected");const int frames=int(std::min(long(kBlock),stop-at));engine.render(audio.data(),frames);
            if(at<event)prefix.write(reinterpret_cast<const char*>(audio.data()),std::streamsize(frames*2*sizeof(float)));
        }
        for(int rank:{0,1})std::cout<<rate<<','<<rank<<','<<engine.getRankAudioState(6,rank).phase<<','<<engine.getRankOscillatorState(6,rank).elapsedSeconds<<'\n';
    }
}
