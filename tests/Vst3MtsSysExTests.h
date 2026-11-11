#pragma once
// Native VST3 DataEvent transport, not direct Engine or APVTS tuning injection.
namespace {
std::vector<float> renderMtsSysEx(Module& module, bool circuit, int channel,
                                 int sub, bool realtime, unsigned mask, bool send)
{
    auto p = cs80::defaultValues();
    p[cs80::P_parityMode] = 0; p[cs80::P_sourcePerformance] = circuit ? 1.f : 0.f;
    p[cs80::P_tuningMode] = 2; p[cs80::P_kasScanTiming] = 0; p[cs80::P_mpeOn] = 0;
    p[cs80::P_drift] = p[cs80::P_voiceDispersion] = p[cs80::P_analogNoise] = p[cs80::P_detune] = 0;
    Instance instance(module, p);
    std::vector<uint8> message {0xF0, static_cast<uint8>(realtime ? 0x7F : 0x7E), 0x7F, 8,
        static_cast<uint8>(sub), static_cast<uint8>(mask >> 14),
        static_cast<uint8>((mask >> 7) & 127), static_cast<uint8>(mask & 127)};
    for (int pc=0;pc<12;++pc)
        if (sub == 8) message.push_back(89); else message.insert(message.end(), {80,0});
    message.push_back(0xF7);
    std::vector<float> output;
    for (int start=0;start<2048;start+=256)
    {
        std::array<float,256> left{},right{}; float* channels[]{left.data(),right.data()};
        AudioBusBuffers bus{}; bus.numChannels=2; bus.channelBuffers32=channels;
        Events events;
        for (int onset : {0,1024}) if (onset>=start && onset<start+256)
        {
            Event e{}; e.type=Event::kNoteOnEvent; e.sampleOffset=onset-start;
            e.noteOn.channel=static_cast<int16>(channel-1); e.noteOn.pitch=onset==0 ? 60 : 64;
            e.noteOn.noteId=e.noteOn.pitch; e.noteOn.velocity=.8f; events.events.push_back(e);
        }
        if (send && start==0)
        {
            Event e{}; e.type=Event::kDataEvent; e.sampleOffset=64;
            e.data.type=DataEvent::kMidiSysEx; e.data.size=static_cast<uint32>(message.size());
            e.data.bytes=message.data(); events.events.push_back(e);
        }
        ProcessContext context{}; context.sampleRate=testSampleRate; context.projectTimeSamples=start;
        ProcessData data{}; data.processMode=testProcessMode; data.symbolicSampleSize=kSample32;
        data.numSamples=256; data.numOutputs=1; data.outputs=&bus;
        data.inputEvents=&events; data.processContext=&context;
        require(instance.audio->process(data)==kResultOk, "VST3 MTS DataEvent process");
        for (int i=0;i<256;++i) { output.push_back(left[i]); output.push_back(right[i]); }
    }
    return output;
}
void testVst3MtsSysEx(Module& module)
{
    int cases=0;
    for (bool circuit : {false,true}) for (int sub : {8,9})
    for (bool realtime : {false,true}) for (int channel : {1,2})
    {
        const auto addressed=renderMtsSysEx(module,circuit,channel,sub,realtime,2,true);
        const auto absent=renderMtsSysEx(module,circuit,channel,sub,realtime,2,false);
        const auto all=renderMtsSysEx(module,circuit,channel,sub,realtime,65535,true);
        check(channel==2 ? addressed==all : addressed==absent,
              "native MTS scale mask selects recipient and preserves nonrecipient audio");
        check(difference(all,absent)>1e-6, "native MTS SysEx changes audio (positive transport control)");
        check(std::equal(all.begin(),all.begin()+128,absent.begin()),
              "native MTS event cannot change samples before offset64");
        if (!realtime)
            check(std::equal(all.begin(),all.begin()+2048,absent.begin()),
                  "native NRT MTS preserves sounding voice until future onset");
        else
            check(regionDifference(all,absent,64,1024)>1e-6,
                  "native RT MTS retunes sounding voice before future onset");
        ++cases;
    }
    std::cout << "Native MTS SysEx DataEvent matrix: " << cases << " cases\n";
}
}
