#include "plugin/PluginProcessor.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

namespace {
int checks=0, failures=0;
void check(bool ok,const char* text) { ++checks; if(!ok) { ++failures; std::cerr<<"FAIL: "<<text<<'\n'; } }
struct Listener : juce::AudioProcessorParameter::Listener {
    int values=0;
    void parameterValueChanged(int,float) override { ++values; }
    void parameterGestureChanged(int,bool) override {}
};
}
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::TemporaryFile fixture("-parameter-admission");
    auto host=std::make_unique<Cs80AudioProcessor>(fixture.getFile());
    for(int key : std::array<int,4>{cs80::P_volume,cs80::P_tuningMode,cs80::P_pitchBendRange,cs80::chParam(0,cs80::CP_hpfCutoff)})
    {
        auto* parameter=host->getAPVTS().getParameter(cs80::paramId(key));
        check(parameter!=nullptr,"fixture parameter exists");
        if(!parameter) continue;
        for(float invalid : {std::numeric_limits<float>::quiet_NaN(),
                            std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()})
        {
            host->setParamValue(key,parameter->convertFrom0to1(.5f));
            const float before=parameter->getValue();
            Listener listener; parameter->addListener(&listener);
            host->setParamValue(key,invalid);
            check(parameter->getValue()==before,"nonfinite physical setter preserves parameter state");
            check(listener.values==0,"nonfinite physical setter emits no host value notification");
            parameter->removeListener(&listener);
        }
        Listener listener; parameter->addListener(&listener);
        host->setParamValue(key,parameter->convertFrom0to1(.25f));
        check(std::isfinite(parameter->getValue()) && listener.values==1,
              "finite setter still changes and notifies parameter");
        parameter->removeListener(&listener);
        const auto& definition=cs80::paramDef(key);
        host->setParamValue(key,definition.min-1000.f);
        check(parameter->getValue()==0.f,"finite below-range input retains lower clamp");
        host->setParamValue(key,definition.max+1000.f);
        check(parameter->getValue()==1.f,"finite above-range input retains upper clamp");
    }
    std::cout<<"Parameter admission: "<<checks<<" checks, "<<failures<<" failures\n";
    return failures ? 1 : 0;
}
