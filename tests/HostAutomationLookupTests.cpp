// The host index is local to one processor. A second instrument has the same
// numeric indices and IDs, but its parameter objects must never borrow queues.
#include "plugin/PluginProcessor.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>

namespace {
int checks=0,errors=0;
void check(bool value,const char* why) {
    ++checks;
    if (!value && errors++ < 8) std::fprintf(stderr,"FAIL: %s\n",why);
}
// The no-curve path must still read every owned parameter even if a VST3
// block has begun and a UI/host edit arrives after that initial read.
void exerciseReadback(Cs80AudioProcessor& processor)
{
    processor.setParamValue(cs80::P_parityMode, 0.0f);
    processor.setParamValue(cs80::P_sourcePerformance, 1.0f);
    processor.prepareToPlay(48000, 4);
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio(2, 4);
    for (int pass = 0; pass < 3; ++pass)
    {
        if (pass != 0) processor.beginVst3HostAutomationBlock(4);
        std::array<double, cs80::kSampleAccurateParameters.size()> expected {};
        for (size_t lane = 0; lane < expected.size(); ++lane)
        {
            auto* parameter = processor.getAPVTS().getParameter(
                Cs80AudioProcessor::parameterId(cs80::kSampleAccurateParameters[lane]));
            const float target = 0.125f + 0.25f * pass + 0.001f * static_cast<float>(lane);
            parameter->setValueNotifyingHost(target);
            expected[lane] = parameter->getValue(); // JUCE physical round trip, not target
        }
        // Still refresh unqueued lanes when a neighboring lane does have a
        // queue, including across the 64-bit activity-word boundary.
        if (pass == 2)
            for (size_t lane : {size_t{0}, size_t{64}, expected.size() - 1})
            {
                auto* parameter = processor.getAPVTS().getParameter(
                    Cs80AudioProcessor::parameterId(cs80::kSampleAccurateParameters[lane]));
                const cs80::HostAutomationQueue queue {nullptr, 1,
                    [](void*, int at, int& frame, double& value) noexcept {
                        frame = 0; value = 0.3125; return at == 0;
                    }};
                check(processor.attachHostAutomationQueue(*parameter, queue), "attach sparse readback lane");
                expected[lane] = 0.3125;
            }
        processor.processBlock(audio, midi);
        for (size_t lane = 0; lane < expected.size(); ++lane)
            check(processor.sampleAccurateParameterValue(lane) == expected[lane],
                  "curve-free and sparse callbacks preserve all public normalized lane values");
        if (pass != 0) processor.endHostAutomationBlock();
    }
}
void exercise(Cs80AudioProcessor& processor, Cs80AudioProcessor& foreign) {
    const auto& roster=processor.getParameters();
    const auto& other=foreign.getParameters();
    int recognised=0;
    std::array<unsigned, cs80::kSampleAccurateParameters.size()> visits{};
    struct Counters {unsigned reads=0, commits=0;} counts;
    const cs80::HostAutomationQueue queue {&counts,1,
        [](void* ptr,int index,int& offset,double& v) noexcept {
            ++static_cast<Counters*>(ptr)->reads;offset=0;v=0.375;return index==0; },
        &counts,[](void* ptr,float v) noexcept {
            if(v==0.375f) ++static_cast<Counters*>(ptr)->commits;
        }};
    processor.beginHostAutomationBlock(33);
    check(roster.size()==other.size(),"matching host roster sizes");
    for(int j=0;j<roster.size();++j) {
        auto* p=roster.getUnchecked(j);
        auto* q=other.getUnchecked(j);
        const auto* identified=dynamic_cast<const juce::AudioProcessorParameterWithID*>(p);
        int expected=-1;
        if(identified) for (size_t i=0;i<visits.size();++i)
            if(identified->paramID == Cs80AudioProcessor::parameterId(cs80::kSampleAccurateParameters[i]))
                { expected=static_cast<int>(i);break; }
        check(p!=q && p->getParameterIndex()==q->getParameterIndex(),"colliding owner indices");
        check(processor.sampleAccurateParameterIndex(*q)==-1,"foreign parameter is not ours");
        check(!processor.attachHostAutomationQueue(*q,queue),"foreign queue cannot attach");
        check(processor.sampleAccurateParameterIndex(*p)==expected,"actual host parameter maps to named lane or -1");
        check(processor.attachHostAutomationQueue(*p,queue)==(expected>=0),"queue eligibility matches named capability");
        if(expected>=0) {++visits[static_cast<size_t>(expected)];++recognised;}
    }
    // A later queue replaces the same lane, without a second endpoint notification.
    auto* first=processor.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(cs80::kSampleAccurateParameters[0]));
    check(first!=nullptr && processor.attachHostAutomationQueue(*first,queue),"same-lane reattachment succeeds");
    // Locally constructed parameter has a negative (not-yet-registered) index.
    juce::AudioParameterFloat unbound(juce::ParameterID{"unused",1},"Unused",0.f,1.f,0.f);
    check(unbound.getParameterIndex()<0 && processor.sampleAccurateParameterIndex(unbound)==-1,"unregistered parameter rejected");
    check(!processor.attachHostAutomationQueue(unbound,queue),"unregistered queue rejected");
    processor.endHostAutomationBlock();
    check(recognised==int(visits.size()) && std::all_of(visits.begin(),visits.end(),[](auto n){ return n==1; }),
          "every sample-accurate lane is present exactly once");
    check(counts.reads==visits.size()+1 && counts.commits==visits.size(),"only owned queues read or commit");
}
}
int main() {
    juce::ScopedJuceInitialiser_GUI init;
    const juce::TemporaryFile fixture;
    auto one=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    auto two=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    exerciseReadback(*one);
    exercise(*one,*two);
    one->prepareToPlay(44100,1);two->prepareToPlay(96000,33);exercise(*one,*two);
    one->releaseResources();two->releaseResources();exercise(*two,*one);
    one->prepareToPlay(48000,127);two->prepareToPlay(44100,64);exercise(*two,*one);
    std::printf("Native processor parameter ownership: %d checks, %d failures\n",checks,errors);
    return errors!=0;
}
