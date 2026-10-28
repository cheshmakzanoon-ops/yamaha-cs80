#include "plugin/PluginEditor.h"
#include "plugin/PluginProcessor.h"
#include "plugin/UiComponents.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_graphics/juce_graphics.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <cstdio>
#include <memory>
#include <atomic>
#include <thread>
#include <chrono>

namespace
{
int failures = 0;

void check(bool condition, const char* what)
{
    if (! condition)
    {
        std::printf("FAIL: %s\n", what);
        ++failures;
    }
}

void validateVisibleControls(juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (! child->isVisible())
            continue;
        // Validate user-facing controls, not internal popup/list viewports
        // which intentionally scroll outside their parent's viewport.
        if (dynamic_cast<cs80::ui::ParamKnob*>(child) != nullptr
            || dynamic_cast<cs80::ui::LedButton*>(child) != nullptr)
        {
            check(! child->getBounds().isEmpty(), "visible panel control has positive bounds");
            check(parent.getLocalBounds().contains(child->getBounds()),
                  "visible panel control is inside parent");
        }
        validateVisibleControls(*child);
    }
}

juce::Button* findButton(juce::Component& parent, const juce::String& text)
{
    for (auto* child : parent.getChildren())
    {
        if (!child->isVisible()) continue;
        if (auto* button = dynamic_cast<juce::Button*>(child))
            if (button->getButtonText() == text) return button;
        if (auto* found = findButton(*child, text)) return found;
    }
    return nullptr;
}

juce::Component* findControl(juce::Component& parent, const juce::String& id)
{
    for (auto* child : parent.getChildren())
    {
        if (child->getComponentID() == id) return child;
        if (auto* found = findControl(*child, id)) return found;
    }
    return nullptr;
}

struct GestureProbe : juce::AudioProcessorParameter::Listener
{
    int begins = 0, ends = 0, changes = 0;
    int depth = 0, unbracketedChanges = 0, malformedGestures = 0;
    void parameterValueChanged(int, float) override
    {
        ++changes;
        if (depth != 1) ++unbracketedChanges;
    }
    void parameterGestureChanged(int, bool starting) override
    {
        if (starting)
        {
            ++begins;
            if (depth != 0) ++malformedGestures;
            ++depth;
        }
        else
        {
            ++ends;
            if (depth != 1) ++malformedGestures;
            --depth;
        }
    }
};

int countHardwareSliders(juce::Component& parent)
{
    int count = 0;
    for (auto* child : parent.getChildren())
    {
        if (!child->isVisible()) continue;
        if (auto* slider = dynamic_cast<juce::Slider*>(child))
            if (slider->getSliderStyle() == juce::Slider::LinearVertical) ++count;
        count += countHardwareSliders(*child);
    }
    return count;
}

void validateKeyboardControls(juce::Component& parent)
{
    for (auto* child : parent.getChildren())
    {
        if (!child->isVisible()) continue;
        if (auto* knob = dynamic_cast<cs80::ui::ParamKnob*>(child))
        {
            auto& slider = knob->getSlider();
            check(slider.getWantsKeyboardFocus(), "visible parameter supports keyboard focus");
            check(slider.getTitle().isNotEmpty(), "visible parameter has accessible title");
        }
        validateKeyboardControls(*child);
    }
}

void flushGui(int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::String outDir = "artifacts/ui";
    bool selfTest = false;
    for (int i = 1; i < argc; ++i)
    {
        if (! juce::String(argv[i]).compareIgnoreCase("--out") && i + 1 < argc)
            outDir = argv[++i];
        else if (! juce::String(argv[i]).compareIgnoreCase("--self-test"))
            selfTest = true;
    }

    juce::File dir(juce::File::getCurrentWorkingDirectory().getChildFile(outDir));
    dir.createDirectory();

    Cs80AudioProcessor processor;
    check(processor.getLatencySamples() == 63, "reported latency at construction");
    processor.prepareToPlay(48000.0, 512);
    check(processor.getLatencySamples() == 63, "reported latency after prepare");

    // Parameter and preset integrity. These are the checks a host would rely
    // on before the instrument is even asked to make a sound.
    check(processor.getFactoryPresets().size() >= 200, "factory bank present");
    for (int index = 0; index < cs80::P_numParams; ++index)
    {
        const auto& def = cs80::paramDef(index);
        check(std::abs(processor.getParamValue(index) - processor.getFactoryPresets()[0].values[index])
                  <= std::max(0.0001f, (def.max - def.min) * 0.00001f),
              "initial program name and actual parameter values agree");
    }
    for (int i = 0; i < cs80::P_numParams; ++i)
    {
        const auto id = Cs80AudioProcessor::parameterId(i);
        check(processor.getAPVTS().getParameter(id) != nullptr, "parameter exposed");
    }

    // Parameter physical values must use the host's skewed mapping. Linear
    // normalization here formerly turned e.g. an 850Hz preset into ~20Hz.
    for (int index = 0; index < cs80::P_numParams; ++index)
    {
        const auto& def = cs80::paramDef(index);
        for (float fraction : { 0.0f, 0.2f, 0.5f, 0.8f, 1.0f })
        {
            const float physical = def.min + fraction * (def.max - def.min);
            processor.setParamValue(index, physical);
            check(std::abs(processor.getParamValue(index) - physical) <= std::max(0.0001f, (def.max - def.min) * 0.00001f),
                  "physical parameter round trips host skew");
        }
    }

    check(processor.getParameters().size() == Cs80AudioProcessor::hostParameterCount
          && Cs80AudioProcessor::hostParameterCount == 229, "229 host parameters");
    for (int i = 0; i < cs80::P_legacyNumParams; ++i)
    {
        check(processor.getParameters()[i] == processor.getAPVTS().getParameter(Cs80AudioProcessor::parameterId(i)),
              "original host parameter order preserved");
        check(processor.getParameters()[i]->getVersionHint() == 1, "legacy AU version hints preserved");
    }
    int next = cs80::P_legacyNumParams;
    for (int channel = 0; channel < 2; ++channel)
    {
        const auto id = Cs80AudioProcessor::hardwareSourceParameterId(channel);
        check(id == "hardware_source_" + juce::String(channel + 1), "stable source parameter ID");
        auto* p = processor.getAPVTS().getParameter(id);
        check(p != nullptr && processor.getParameters()[next++] == p, "source appended before memories");
        if (p)
        {
            check(p->getVersionHint() == 2, "new source AU version hint follows legacy parameters");
            check(p->getNumSteps() == 14 && p->convertFrom0to1(p->getDefaultValue()) == 13.0f,
                  "source choices and PANEL default");
        }
    }
    for (int bank = 0; bank < 4; ++bank)
        for (int local = 0; local < cs80::CP_count; ++local)
        {
            const auto id = Cs80AudioProcessor::memoryParameterId(bank, local);
            if (!Cs80AudioProcessor::isMemoryControl(local))
            {
                check(id.isEmpty(), "extensions excluded from miniature automation");
                continue;
            }
            check(id == "memory_" + juce::String(bank + 1) + "_" + cs80::kChannelDefs[local].id,
                  "stable descriptive memory parameter ID");
            auto* p = processor.getAPVTS().getParameter(id);
            check(p != nullptr && processor.getParameters()[next++] == p, "memory bank parameter ordering");
            if (!p) continue;
            check(p->getVersionHint() == 2, "new memory AU version hint follows legacy parameters");
            const auto& d = cs80::kChannelDefs[local];
            const auto& range = p->getNormalisableRange();
            check(range.start == d.min && range.end == d.max && range.skew == d.skew,
                  "memory range and skew agree with main panel");
            for (float fraction : { 0.0f, 0.2f, 0.5f, 0.8f, 1.0f })
            {
                const float value = local == cs80::CP_saw || local == cs80::CP_pulse
                    ? (fraction >= 0.5f ? 1.0f : 0.0f) : d.min + fraction * (d.max - d.min);
                processor.setMemoryValue(bank, local, value);
                check(std::abs(processor.getMemoryValue(bank, local) - value)
                          <= std::max(0.0001f, (d.max - d.min) * 0.00001f), "memory API uses host skew");
            }
            processor.setMemoryValue(bank, local, d.def);
        }


    // Literal snapshot of the shipped228 IDs. This must not be generated from
    // the live definitions, or accidental reordering could make both sides agree.
    const char* legacyHostIds[] = {
        "master_tune",
        "detune",
        "mix",
        "brilliance",
        "resonance",
        "master_sustain",
        "volume",
        "init_bend",
        "portamento",
        "glide_mode",
        "sustain_mode",
        "key_release",
        "ribbon",
        "sub_wave",
        "sub_speed",
        "sub_vco",
        "sub_vcf",
        "sub_vca",
        "sub_at_speed",
        "sub_at_vco",
        "ring_speed",
        "ring_depth",
        "ring_attack",
        "ring_decay",
        "ring_amount",
        "chorus_on",
        "chorus_mode",
        "chorus_speed",
        "chorus_depth",
        "output_mode",
        "voice_dispersion",
        "drift",
        "analog_noise",
        "mpe_on",
        "pitch_bend_range",
        "mpe_bend_range",
        "tuning_ref",
        "modwheel_sub",
        "modwheel_vcf",
        "lfo1_rate",
        "lfo1_depth",
        "lfo1_dest",
        "lfo2_rate",
        "lfo2_depth",
        "lfo2_dest",
        "lfo3_rate",
        "lfo3_depth",
        "lfo3_dest",
        "fx1_type",
        "fx1_mix",
        "fx2_type",
        "fx2_mix",
        "fx3_type",
        "fx3_mix",
        "arp_on",
        "arp_mode",
        "arp_rate",
        "arp_octaves",
        "macro1",
        "macro2",
        "macro3",
        "macro4",
        "ch1_saw",
        "ch1_pulse",
        "ch1_sine",
        "ch1_noise",
        "ch1_pw",
        "ch1_pwm_speed",
        "ch1_pwm_depth",
        "ch1_feet",
        "ch1_hpf_cutoff",
        "ch1_hpf_res",
        "ch1_hpf_on",
        "ch1_lpf_cutoff",
        "ch1_lpf_res",
        "ch1_lpf_on",
        "ch1_f_il",
        "ch1_f_al",
        "ch1_f_a",
        "ch1_f_d",
        "ch1_f_r",
        "ch1_vcf_level",
        "ch1_v_a",
        "ch1_v_d",
        "ch1_v_s",
        "ch1_v_r",
        "ch1_v_level",
        "ch1_init_brill",
        "ch1_init_level",
        "ch1_after_brill",
        "ch1_after_level",
        "ch1_pan",
        "ch2_saw",
        "ch2_pulse",
        "ch2_sine",
        "ch2_noise",
        "ch2_pw",
        "ch2_pwm_speed",
        "ch2_pwm_depth",
        "ch2_feet",
        "ch2_hpf_cutoff",
        "ch2_hpf_res",
        "ch2_hpf_on",
        "ch2_lpf_cutoff",
        "ch2_lpf_res",
        "ch2_lpf_on",
        "ch2_f_il",
        "ch2_f_al",
        "ch2_f_a",
        "ch2_f_d",
        "ch2_f_r",
        "ch2_vcf_level",
        "ch2_v_a",
        "ch2_v_d",
        "ch2_v_s",
        "ch2_v_r",
        "ch2_v_level",
        "ch2_init_brill",
        "ch2_init_level",
        "ch2_after_brill",
        "ch2_after_level",
        "ch2_pan",
        "hardware_source_1",
        "hardware_source_2",
        "memory_1_saw",
        "memory_1_pulse",
        "memory_1_sine",
        "memory_1_noise",
        "memory_1_pw",
        "memory_1_pwm_speed",
        "memory_1_pwm_depth",
        "memory_1_hpf_cutoff",
        "memory_1_hpf_res",
        "memory_1_lpf_cutoff",
        "memory_1_lpf_res",
        "memory_1_f_il",
        "memory_1_f_al",
        "memory_1_f_a",
        "memory_1_f_d",
        "memory_1_f_r",
        "memory_1_vcf_level",
        "memory_1_v_a",
        "memory_1_v_d",
        "memory_1_v_s",
        "memory_1_v_r",
        "memory_1_v_level",
        "memory_1_init_brill",
        "memory_1_init_level",
        "memory_1_after_brill",
        "memory_1_after_level",
        "memory_2_saw",
        "memory_2_pulse",
        "memory_2_sine",
        "memory_2_noise",
        "memory_2_pw",
        "memory_2_pwm_speed",
        "memory_2_pwm_depth",
        "memory_2_hpf_cutoff",
        "memory_2_hpf_res",
        "memory_2_lpf_cutoff",
        "memory_2_lpf_res",
        "memory_2_f_il",
        "memory_2_f_al",
        "memory_2_f_a",
        "memory_2_f_d",
        "memory_2_f_r",
        "memory_2_vcf_level",
        "memory_2_v_a",
        "memory_2_v_d",
        "memory_2_v_s",
        "memory_2_v_r",
        "memory_2_v_level",
        "memory_2_init_brill",
        "memory_2_init_level",
        "memory_2_after_brill",
        "memory_2_after_level",
        "memory_3_saw",
        "memory_3_pulse",
        "memory_3_sine",
        "memory_3_noise",
        "memory_3_pw",
        "memory_3_pwm_speed",
        "memory_3_pwm_depth",
        "memory_3_hpf_cutoff",
        "memory_3_hpf_res",
        "memory_3_lpf_cutoff",
        "memory_3_lpf_res",
        "memory_3_f_il",
        "memory_3_f_al",
        "memory_3_f_a",
        "memory_3_f_d",
        "memory_3_f_r",
        "memory_3_vcf_level",
        "memory_3_v_a",
        "memory_3_v_d",
        "memory_3_v_s",
        "memory_3_v_r",
        "memory_3_v_level",
        "memory_3_init_brill",
        "memory_3_init_level",
        "memory_3_after_brill",
        "memory_3_after_level",
        "memory_4_saw",
        "memory_4_pulse",
        "memory_4_sine",
        "memory_4_noise",
        "memory_4_pw",
        "memory_4_pwm_speed",
        "memory_4_pwm_depth",
        "memory_4_hpf_cutoff",
        "memory_4_hpf_res",
        "memory_4_lpf_cutoff",
        "memory_4_lpf_res",
        "memory_4_f_il",
        "memory_4_f_al",
        "memory_4_f_a",
        "memory_4_f_d",
        "memory_4_f_r",
        "memory_4_vcf_level",
        "memory_4_v_a",
        "memory_4_v_d",
        "memory_4_v_s",
        "memory_4_v_r",
        "memory_4_v_level",
        "memory_4_init_brill",
        "memory_4_init_level",
        "memory_4_after_brill",
        "memory_4_after_level"
    };
    for (int i=0; i<228; ++i)
        check(processor.getParameters()[i] == processor.getAPVTS().getParameter(legacyHostIds[i]),
              "all previously shipped host parameter IDs/order preserved");
    auto* atVcf = processor.getAPVTS().getParameter("sub_at_vcf");
    check(next == 228 && atVcf != nullptr && processor.getParameters()[next] == atVcf,
          "AT VCF appended after all programmer parameters");
    check(atVcf != nullptr && atVcf->getVersionHint() == 3, "AT VCF AU hint follows programmer hint2");
    if (atVcf)
    {
        check(atVcf->getDefaultValue() == 0.0f, "AT VCF defaults off");
        atVcf->setValueNotifyingHost(.65f);
        juce::AudioBuffer<float> atAudio(2,64);
        juce::MidiBuffer atMidi;
        processor.processBlock(atAudio,atMidi);
        check(std::abs(processor.getEngine().getParams()[cs80::P_subAtVcf]-.65f)<.00001f,
              "AT VCF host automation reaches core without an editor");
        juce::MemoryBlock savedAt;
        processor.getStateInformation(savedAt);
        Cs80AudioProcessor restoredAt;
        restoredAt.setStateInformation(savedAt.getData(), (int)savedAt.getSize());
        check(std::abs(restoredAt.getParamValue(cs80::P_subAtVcf)-.65f)<.00001f,
              "AT VCF state round trip");
        auto oldState=processor.getAPVTS().copyState();
        oldState.removeChild(oldState.getChildWithProperty("id","sub_at_vcf"),nullptr);
        auto xml=oldState.createXml();
        const auto text=xml->toString();
        restoredAt.setStateInformation(text.toRawUTF8(),text.getNumBytesAsUTF8());
        check(restoredAt.getParamValue(cs80::P_subAtVcf)==0.0f,
              "old state defaults missing AT VCF rather than retaining live value");
        atVcf->setValueNotifyingHost(0.0f);
    }

    processor.loadFactoryPreset(22);
    check(processor.getPresetName() == "Replicant Brass", "factory preset load");
    check(processor.getCurrentPresetIndex() == 22, "factory preset index");

    // Host state round-trip.
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    Cs80AudioProcessor restored;
    restored.prepareToPlay(48000.0, 512);
    restored.setStateInformation(state.getData(), (int) state.getSize());
    check(restored.getPresetName() == "Replicant Brass", "state round-trip preset name");
    check(restored.getCurrentPresetIndex() == 22, "state round-trip preset index");
    check(std::abs(restored.getParamValue(cs80::P_mix)
                   - processor.getParamValue(cs80::P_mix)) < 1.0e-5f,
          "state round-trip parameter");

    // Audio path, including MIDI delivery through the JUCE buffer.
    {
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8) 100), 0);
        midi.addEvent(juce::MidiMessage::noteOn(1, 64, (juce::uint8) 100), 0);
        juce::AudioBuffer<float> buffer(2, 1024);
        buffer.clear();
        processor.processBlock(buffer, midi);
        check(buffer.getMagnitude(0, 0, buffer.getNumSamples()) > 0.0001f,
              "plugin renders MIDI");
        check(processor.getActiveVoiceCount() >= 2, "plugin allocates voices");
        processor.getEngine().allNotesOff(false);
    }

    // Channel-mode messages must not be swallowed by the generic CC branch.
    for (int controller : { 120, 123 })
    {
        Cs80AudioProcessor panic;
        auto values = cs80::defaultValues();
        values[cs80::P_keyRelease] = 0.001f;
        for (int channel = 0; channel < 2; ++channel)
        {
            values[cs80::chParam(channel, cs80::CP_vR)] = 0.001f;
            values[cs80::chParam(channel, cs80::CP_fR)] = 0.001f;
        }
        panic.applyParameterValues(values);
        panic.prepareToPlay(48000.0, 256);
        juce::AudioBuffer<float> audio(2, 256);
        juce::MidiBuffer messages;
        messages.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)100), 0);
        messages.addEvent(juce::MidiMessage::noteOn(2, 67, (juce::uint8)100), 0);
        panic.processBlock(audio, messages);
        messages.clear();
        messages.addEvent(juce::MidiMessage::controllerEvent(1, controller, 0), 0);
        panic.processBlock(audio, messages);
        messages.clear();
        for (int block = 0; block < 200; ++block) panic.processBlock(audio, messages);
        int channel1 = 0, channel2 = 0;
        for (int voice = 0; voice < 8; ++voice)
        {
            const auto info = panic.getEngine().getVoiceInfo(voice);
            if (info.active && info.channel == 1) ++channel1;
            if (info.active && info.channel == 2) ++channel2;
        }
        check(channel1 == 0, "actual MIDI CC120/123 clears addressed channel without sticky voices");
        check(channel2 == 1, "actual MIDI CC120/123 preserves other MIDI channel");
        messages.addEvent(juce::MidiMessage::controllerEvent(2, controller, 0), 0);
        panic.processBlock(audio, messages);
        messages.clear();
        for (int block = 0; block < 200; ++block) panic.processBlock(audio, messages);
        check(panic.getActiveVoiceCount() == 0, "actual MIDI panic clears remaining channel");
    }

    // GUI control mailboxes are consumed on the audio thread, including a
    // host block exceeding prepareToPlay's advertised size.
    {
        processor.setRibbonFromUI(0.2f);
        processor.setPitchWheelFromUI(-0.1f);
        processor.setModWheelFromUI(0.5f);
        juce::AudioBuffer<float> large(2, 10003);
        juce::MidiBuffer events;
        events.addEvent(juce::MidiMessage::noteOn(1, 57, (juce::uint8) 100), 4095);
        events.addEvent(juce::MidiMessage::noteOff(1, 57), 8193);
        processor.processBlock(large, events);
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < large.getNumSamples(); ++i)
                finite = finite && std::isfinite(large.getSample(ch, i));
        check(finite, "oversized host block stays finite");
        check(large.getMagnitude(0, 4095, 4098) > 0.0001f, "oversized block MIDI renders after chunk boundary");
        processor.setRibbonFromUI(0.0f);
        processor.setPitchWheelFromUI(0.0f);
        processor.setModWheelFromUI(0.0f);
        processor.getEngine().allNotesOff(false);
    }

    // Physical programmer memories are independent controls, not destructive
    // copies over the main-panel APVTS. Test actual effective audio settings.
    {
        Cs80AudioProcessor memories;
        memories.prepareToPlay(48000.0, 64);
        const int cutoff = cs80::chParam(0, cs80::CP_lpfCutoff);
        memories.setParamValue(cutoff, 3400.0f);
        memories.setParamValue(cs80::chParam(0, cs80::CP_feet), 2.0f);
        memories.setParamValue(cs80::chParam(0, cs80::CP_pan), 0.2f);
        for (int bank = 0; bank < 4; ++bank)
            memories.setMemoryValue(bank, cs80::CP_lpfCutoff, 500.0f + 300.0f * (float) bank);
        memories.selectHardwareSource(0, 11);
        memories.selectHardwareSource(1, 12);
        juce::AudioBuffer<float> audio(2, 64);
        juce::MidiBuffer noMidi;
        memories.processBlock(audio, noMidi);
        check(std::abs(memories.getEngine().getParams()[cutoff] - 500.0f) < 0.01f, "memory 1 drives rank I");
        check(std::abs(memories.getEngine().getParams()[cs80::chParam(1, cs80::CP_lpfCutoff)] - 1400.0f) < 0.01f, "memory 4 drives rank II");
        check(std::abs(memories.getParamValue(cutoff) - 3400.0f) < 0.01f, "memory recall preserves physical panel");
        check(memories.getEngine().getParams()[cs80::chParam(0, cs80::CP_feet)] == 2.0f, "memory leaves footage live");
        check(std::abs(memories.getEngine().getParams()[cs80::chParam(0, cs80::CP_pan)] - 0.2f) < 0.001f, "memory leaves modern pan live");
        juce::MemoryBlock memoryState;
        memories.getStateInformation(memoryState);
        Cs80AudioProcessor recalled;
        recalled.setStateInformation(memoryState.getData(), (int) memoryState.getSize());
        check(recalled.getHardwareSource(0) == 11 && recalled.getHardwareSource(1) == 12, "hardware source selectors round trip");
        for (int bank = 0; bank < 4; ++bank)
            check(std::abs(recalled.getMemoryValue(bank, cs80::CP_lpfCutoff) - (500.0f + 300.0f * (float) bank)) < 0.01f, "independent programmer memory round trip");
        memories.selectHardwareSource(0, 13);
        memories.processBlock(audio, noMidi);
        check(std::abs(memories.getEngine().getParams()[cutoff] - 3400.0f) < 0.01f, "PANEL restores untouched manual controls");
        memories.selectHardwareSource(0, 2);
        memories.processBlock(audio, noMidi);
        cs80::ParamValues brass;
        cs80::applyHardwareTone(brass, 0, 2);
        check(std::abs(memories.getEngine().getParams()[cutoff] - brass[cutoff]) < 0.01f, "hardware factory selector drives effective voice");
        check(std::abs(memories.getParamValue(cutoff) - 3400.0f) < 0.01f, "factory selection preserves PANEL settings");
        memories.loadFactoryPreset(22);
        check(memories.getHardwareSource(0) == 13 && memories.getHardwareSource(1) == 13, "digital program recall resets selectors to PANEL");
        const float beforeInvalid = memories.getMemoryValue(0, cs80::CP_lpfCutoff);
        memories.setMemoryValue(0, cs80::CP_lpfCutoff, std::numeric_limits<float>::quiet_NaN());
        check(memories.getMemoryValue(0, cs80::CP_lpfCutoff) == beforeInvalid, "programmer rejects nonfinite edits");
    }

    // Host automation is authoritative, including when no editor exists.
    {
        Cs80AudioProcessor automated;
        automated.prepareToPlay(48000.0, 64);
        auto setHost = [&](const juce::String& id, float value)
        {
            auto* p = automated.getAPVTS().getParameter(id);
            check(p != nullptr, "automated parameter exists");
            if (p) p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < cs80::CP_count; ++local)
                if (Cs80AudioProcessor::isMemoryControl(local))
                {
                    const auto& d = cs80::kChannelDefs[local];
                    const float fraction = (float)(bank + 1) / 5.0f;
                    setHost(Cs80AudioProcessor::memoryParameterId(bank, local),
                        local == cs80::CP_saw || local == cs80::CP_pulse ? (float)(bank % 2)
                        : d.min + fraction * (d.max - d.min));
                }
        setHost(Cs80AudioProcessor::hardwareSourceParameterId(0), 11);
        setHost(Cs80AudioProcessor::hardwareSourceParameterId(1), 12);
        juce::AudioBuffer<float> audio(2, 64);
        juce::MidiBuffer midi;
        automated.processBlock(audio, midi);
        for (int channel = 0; channel < 2; ++channel)
            check(std::abs(automated.getEngine().getParams()[cs80::chParam(channel, cs80::CP_lpfCutoff)]
                  - automated.getMemoryValue(channel == 0 ? 0 : 3, cs80::CP_lpfCutoff)) < 0.02f,
                  "host automation drives selected effective audio parameters");
        juce::MemoryBlock saved;
        automated.getStateInformation(saved);
        Cs80AudioProcessor copy;
        copy.setStateInformation(saved.getData(), (int)saved.getSize());
        check(copy.getHardwareSource(0) == 11 && copy.getHardwareSource(1) == 12, "automated selectors round trip");
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < cs80::CP_count; ++local)
                if (Cs80AudioProcessor::isMemoryControl(local))
                    check(std::abs(copy.getMemoryValue(bank, local) - automated.getMemoryValue(bank, local))
                         <= std::max(0.0001f, cs80::kChannelDefs[local].max * 0.00001f),
                         "all 104 memory parameters round trip");
        const float retained = automated.getMemoryValue(2, cs80::CP_lpfCutoff);
        automated.loadFactoryPreset(0);
        check(automated.getHardwareSource(0) == 13 && automated.getHardwareSource(1) == 13
              && automated.getMemoryValue(2, cs80::CP_lpfCutoff) == retained,
              "digital preset selects PANEL without erasing miniature controls");

        auto legacy = copy.getAPVTS().copyState();
        for (int i = legacy.getNumChildren(); --i >= 0;)
        {
            const auto id = legacy.getChild(i).getProperty("id").toString();
            if (id.startsWith("memory_") || id.startsWith("hardware_source_"))
                legacy.removeChild(i, nullptr);
        }
        juce::ValueTree hardware("HARDWARE");
        hardware.setProperty("version", 1, nullptr);
        hardware.setProperty("sourceI", 12, nullptr);
        hardware.setProperty("sourceII", 11, nullptr);
        for (int bank = 0; bank < 4; ++bank)
        {
            juce::ValueTree memory("MEMORY");
            memory.setProperty("bank", bank, nullptr);
            memory.setProperty("p" + juce::String(cs80::CP_lpfCutoff), 800.0f + 100.0f * (float)bank, nullptr);
            hardware.addChild(memory, -1, nullptr);
        }
        legacy.addChild(hardware, -1, nullptr);
        auto restoreTree = [&](const juce::ValueTree& tree)
        {
            auto xml = tree.createXml();
            const auto text = xml->toString();
            copy.setStateInformation(text.toRawUTF8(), text.getNumBytesAsUTF8());
        };
        restoreTree(legacy);
        check(copy.getHardwareSource(0) == 12 && copy.getHardwareSource(1) == 11, "version 1 selectors migrate");
        for (int bank = 0; bank < 4; ++bank)
            check(std::abs(copy.getMemoryValue(bank, cs80::CP_lpfCutoff) - (800.0f + 100.0f * (float)bank)) < 0.02f,
                  "version 1 four memories migrate");
        check(!copy.getAPVTS().copyState().getChildWithName("HARDWARE").isValid(), "legacy duplicate authority removed");
        legacy.removeChild(hardware, nullptr);
        restoreTree(legacy);
        check(copy.getHardwareSource(0) == 13 && copy.getHardwareSource(1) == 13, "pre-programmer state defaults PANEL");
        for (int bank = 0; bank < 4; ++bank)
            for (int local = 0; local < cs80::CP_count; ++local)
                if (Cs80AudioProcessor::isMemoryControl(local))
                    check(std::abs(copy.getMemoryValue(bank, local) - cs80::kChannelDefs[local].def)
                          <= std::max(0.0001f, cs80::kChannelDefs[local].max * 0.00001f),
                          "pre-programmer state resets every missing mini control");
        auto partial = copy.getAPVTS().copyState();
        const auto missingId = Cs80AudioProcessor::memoryParameterId(0, cs80::CP_lpfCutoff);
        partial.removeChild(partial.getChildWithProperty("id", missingId), nullptr);
        copy.setMemoryValue(0, cs80::CP_lpfCutoff, 111.0f);
        restoreTree(partial);
        check(std::abs(copy.getMemoryValue(0, cs80::CP_lpfCutoff) - cs80::kChannelDefs[cs80::CP_lpfCutoff].def) < 0.02f,
              "missing new-state parameter does not retain stale live value");
    }

    // Host state callbacks can run off the message thread while an editor is
    // being opened or closed. No processor callback may capture that editor.
    // This stress regression is not a substitute for ThreadSanitizer coverage.
    {
        Cs80AudioProcessor concurrent;
        concurrent.prepareToPlay(48000.0, 64);
        auto makeState = [&](const juce::String& name, int index)
        {
            auto tree = concurrent.getAPVTS().copyState();
            tree.setProperty("presetName", name, nullptr);
            tree.setProperty("factoryIndex", index, nullptr);
            return tree.createXml()->toString();
        };
        const auto stateA = makeState("Thread State A", 7);
        const auto stateB = makeState("Thread State B", 11);
        auto restore = [&](const juce::String& state)
        {
            concurrent.setStateInformation(state.toRawUTF8(), state.getNumBytesAsUTF8());
        };
        auto validPair = [](const Cs80AudioProcessor::PresetMetadata& metadata)
        {
            return (metadata.name == "Thread State A" && metadata.factoryIndex == 7)
                || (metadata.name == "Thread State B" && metadata.factoryIndex == 11);
        };
        restore(stateA);
        std::atomic<bool> stop { false }, valid { true };
        std::atomic<int> iterations { 0 };
        std::thread writer([&]
        {
            while (!stop.load(std::memory_order_acquire))
            {
                restore((iterations.load(std::memory_order_relaxed) & 1) ? stateA : stateB);
                if (!validPair(concurrent.getPresetMetadata()))
                    valid.store(false, std::memory_order_relaxed);
                iterations.fetch_add(1, std::memory_order_release);
            }
        });
        while (iterations.load(std::memory_order_acquire) == 0)
            std::this_thread::yield();
        for (int cycle = 0; cycle < 8; ++cycle)
        {
            {
                auto temporaryEditor = std::make_unique<Cs80AudioProcessorEditor>(concurrent);
                temporaryEditor->setSize(1120, 700);
                temporaryEditor->setVisible(true);
                for (int read = 0; read < 100; ++read)
                {
                    if (!validPair(concurrent.getPresetMetadata()))
                        valid.store(false, std::memory_order_relaxed);
                    const int program = concurrent.getCurrentProgram();
                    if (program != 7 && program != 11)
                        valid.store(false, std::memory_order_relaxed);
                }
                // Exercise serialization while background state replacement runs.
                juce::MemoryBlock state;
                concurrent.getStateInformation(state);
                const auto text = juce::String::fromUTF8(
                    static_cast<const char*>(state.getData()), (int)state.getSize());
                if (auto xml = juce::XmlDocument::parse(text))
                {
                    const auto tree = juce::ValueTree::fromXml(*xml);
                    if (!validPair({ tree.getProperty("presetName").toString(),
                                     (int)tree.getProperty("factoryIndex") }))
                        valid.store(false, std::memory_order_relaxed);
                }
                else valid.store(false, std::memory_order_relaxed);
                flushGui(20);
            }
            flushGui(20); // Process pending attachment work after editor destruction.
        }
        stop.store(true, std::memory_order_release);
        writer.join();
        check(iterations.load() > 0 && valid.load(),
              "background state restore preserves coherent metadata through editor lifecycles");

        const auto nameBeforeRename = concurrent.getPresetName();
        Cs80AudioProcessorEditor timerEditor(concurrent);
        timerEditor.setVisible(true);
        auto findLabelWithText = [](auto&& self, juce::Component& component,
                                    const juce::String& text) -> juce::Label*
        {
            if (auto* label = dynamic_cast<juce::Label*>(&component))
                if (label->getText() == text) return label;
            for (auto* child : component.getChildren())
                if (auto* found = self(self, *child, text)) return found;
            return nullptr;
        };
        juce::Component::SafePointer<juce::Label> presetDisplay(
            findLabelWithText(findLabelWithText, timerEditor, nameBeforeRename));
        check(presetDisplay != nullptr, "timer regression locates existing preset label");
        const juce::String renamed = "Timer Refresh Verified";
        std::thread rename([&] { concurrent.setPresetName(renamed); });
        rename.join();
        check(concurrent.getPresetName() == renamed,
              "background rename updates processor metadata before UI polling");

        // JUCE timers are best-effort message-thread work, not a synchronous
        // 125ms guarantee. Observe the actual label while pumping real events.
        // Do not call tick/syncPresetDisplay directly or replace this with sleep.
        const auto started = std::chrono::steady_clock::now();
        const auto deadline = started + std::chrono::seconds(2);
        while (presetDisplay != nullptr && presetDisplay->getText() != renamed
               && std::chrono::steady_clock::now() < deadline)
            flushGui(10);
        const double elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        const auto observed = presetDisplay != nullptr
            ? presetDisplay->getText() : juce::String("<missing>");
        const auto modelName = concurrent.getPresetName();
        std::printf("Preset timer refresh: elapsed=%.1fms model='%s' label='%s' stateWrites=%d\n",
                    elapsedMs, modelName.toRawUTF8(), observed.toRawUTF8(), iterations.load());
        check(presetDisplay != nullptr && observed == renamed,
              "message-thread timer refreshes preset name after background update");

    }

    Cs80AudioProcessorEditor editor(processor);
    editor.setSize(1600, 1000);
    editor.setVisible(true);
    if (auto* atSlider=dynamic_cast<juce::Slider*>(findControl(editor,"sub_at_vcf")))
    {
        check(atSlider->getTitle()=="AT VCF", "global extension is not mislabeled Channel II");
        auto* p=processor.getAPVTS().getParameter("sub_at_vcf");
        p->setValueNotifyingHost(.4f);
        flushGui(30);
        check(std::abs(atSlider->getValue()-.4)<.00001, "AT VCF host automation updates visible slider");
        GestureProbe probe;
        p->addListener(&probe);
        const double before=atSlider->getValue();
        atSlider->keyPressed(juce::KeyPress(juce::KeyPress::upKey));
        check(atSlider->getValue()>before && probe.begins==1 && probe.ends==1
              && probe.unbracketedChanges==0 && probe.malformedGestures==0,
              "AT VCF actual keyboard edit sends one balanced host gesture");
        p->removeListener(&probe);
        p->setValueNotifyingHost(0.0f);
    }
    else check(false, "AT VCF main-panel slider exists");
    flushGui(150);

    auto save = [&](const juce::String& name) {
        validateVisibleControls(editor);
        auto image = editor.createComponentSnapshot(editor.getLocalBounds(), true, 1.0f);
        check(! image.isNull(), "snapshot image created");
        check(image.getWidth() == editor.getWidth(), "snapshot width");
        check(image.getHeight() == editor.getHeight(), "snapshot height");
        auto file = dir.getChildFile(name + ".png");
        file.deleteFile();
        juce::FileOutputStream stream(file);
        juce::PNGImageFormat png;
        if (stream.openedOk() && png.writeImageToStream(image, stream))
            std::printf("wrote %s\n", file.getFullPathName().toRawUTF8());
        else
        {
            std::printf("FAILED %s\n", file.getFullPathName().toRawUTF8());
            ++failures;
        }
    };

    validateKeyboardControls(editor);
    check(countHardwareSliders(editor) >= 40, "main instrument uses hardware slider banks");
    save("main-1600x1000");

    editor.setSize(1200, 760);
    flushGui(100);
    save("main-1200x760");

    editor.setSize(1120, 700);
    flushGui(100);
    save("main-1120x700");

    editor.setSize(1600, 1000);
    editor.showAdvancedPanel(true);
    check(editor.isAdvancedVisible(), "advanced panel opens");
    for (int page = 0; page < 4; ++page)
    {
        editor.showAdvancedPage(page);
        flushGui(80);
        save("advanced-page" + juce::String(page + 1));
    }

    editor.showPresetBrowser(true);
    check(! editor.isAdvancedVisible(), "browser replaces advanced panel");
    flushGui(100);
    save("browser-1600x1000");
    editor.showPresetBrowser(false);
    // Repeated overlay transitions must restore the instrument surface.
    for (int pass = 0; pass < 3; ++pass)
    {
        editor.showAdvancedPanel(true);
        check(editor.isAdvancedVisible(), "advanced panel reopens");
        editor.showPresetBrowser(true);
        check(! editor.isAdvancedVisible(), "browser hides advanced on repeated transition");
        editor.showPresetBrowser(false);
        editor.showAdvancedPanel(false);
        check(! editor.isAdvancedVisible(), "advanced panel closes");
    }
    flushGui(80);
    save("main-restored-1600x1000");

    if (auto* lid = findButton(editor, "OPEN MEMORY LID"))
    {
        lid->triggerClick();
        flushGui(100);
        check(countHardwareSliders(editor) >= 96, "all four miniature programmer banks exposed");
        const auto sliderId = Cs80AudioProcessor::memoryParameterId(0, cs80::CP_lpfCutoff);
        auto* memoryParameter = processor.getAPVTS().getParameter(sliderId);
        if (auto* slider = dynamic_cast<juce::Slider*>(findControl(editor, sliderId)))
        {
            GestureProbe probe;
            memoryParameter->addListener(&probe);
            const double original = slider->getValue();
            // This renderer deliberately has no native desktop peer. JUCE
            // Component::getAccessibilityHandler returns null without a window
            // handle. Exercise the same scoped value-edit transaction here;
            // actual arrow-key actions below independently test user gestures.
            {
                juce::Slider::ScopedDragNotification gesture(*slider);
                slider->setValue(1234.0, juce::sendNotificationSync);
            }
            check(std::abs(processor.getMemoryValue(0, cs80::CP_lpfCutoff) - 1234.0f) < 0.02f,
                  "miniature GUI slider writes APVTS");
            check(probe.changes > 0 && probe.begins == 1 && probe.ends == 1
                  && probe.depth == 0 && probe.unbracketedChanges == 0 && probe.malformedGestures == 0,
                  "miniature scoped value edit sends exactly one balanced host gesture");
            memoryParameter->setValueNotifyingHost(memoryParameter->convertTo0to1(2456.0f));
            flushGui(30);
            check(std::abs(slider->getValue() - 2456.0) < 0.02, "host automation updates miniature slider");
            memoryParameter->removeListener(&probe);
            for (bool duringDrag : { false, true })
            {
                GestureProbe keyboardProbe;
                memoryParameter->addListener(&keyboardProbe);
                const double beforeKey = slider->getValue();
                if (duringDrag)
                {
                    // The key must join the existing drag, not start a nested
                    // host gesture. This exercises FocusSlider's guard.
                    juce::Slider::ScopedDragNotification drag(*slider);
                    check(slider->keyPressed(juce::KeyPress(juce::KeyPress::upKey)),
                          "miniature slider handles arrow during drag");
                }
                else
                    check(slider->keyPressed(juce::KeyPress(juce::KeyPress::upKey)),
                          "miniature slider handles arrow keyboard edit");
                check(slider->getValue() > beforeKey
                      && std::abs(processor.getMemoryValue(0, cs80::CP_lpfCutoff) - slider->getValue()) < 0.02,
                      "actual arrow key changes miniature APVTS value");
                check(keyboardProbe.changes > 0 && keyboardProbe.begins == 1 && keyboardProbe.ends == 1
                      && keyboardProbe.depth == 0 && keyboardProbe.unbracketedChanges == 0
                      && keyboardProbe.malformedGestures == 0,
                      "arrow edit has one nonnested balanced host gesture");
                memoryParameter->removeListener(&keyboardProbe);
            }
            memoryParameter->setValueNotifyingHost(memoryParameter->convertTo0to1((float) original));
        }
        else check(false, "miniature slider discoverable");
        const auto toggleId = Cs80AudioProcessor::memoryParameterId(0, cs80::CP_saw);
        if (auto* toggle = dynamic_cast<juce::Button*>(findControl(editor, toggleId)))
        {
            auto* parameter = processor.getAPVTS().getParameter(toggleId);
            const bool original = toggle->getToggleState();
            GestureProbe probe;
            parameter->addListener(&probe);
            toggle->triggerClick();
            flushGui(80);
            check(toggle->getToggleState() != original
                  && processor.getMemoryValue(0, cs80::CP_saw) == (original ? 0.0f : 1.0f),
                  "miniature switch writes APVTS");
            check(probe.changes > 0 && probe.begins > 0 && probe.begins == probe.ends,
                  "miniature switch sends balanced host gestures");
            parameter->removeListener(&probe);
            parameter->setValueNotifyingHost(original ? 1.0f : 0.0f);
            flushGui(30);
            check(toggle->getToggleState() == original, "host automation updates miniature switch");
        }
        else check(false, "miniature switch discoverable");
        save("memory-programmer-1600x1000");
        if (auto* close = findButton(editor, "CLOSE LID"))
        {
            close->triggerClick();
            flushGui(80);
        }
        else check(false, "memory lid has working close control");
        check(findButton(editor, "OPEN MEMORY LID") != nullptr, "memory lid reopens after close");
    }
    else check(false, "physical memory lid can be opened");

    // User preset save/load path. This touches the per-user application data
    // directory, which is created on demand by the processor.
    const juce::String smokePresetName = "UI Smoke Preset " + juce::Uuid().toString();
    processor.saveUserPreset(smokePresetName);
    const auto files = processor.getUserPresetFiles();
    bool found = false;
    for (const auto& file : files)
        found = found || file.getFileNameWithoutExtension() == smokePresetName;
    check(found, "user preset saved");
    if (found)
    {
        processor.loadFactoryPreset(0);
        for (const auto& file : processor.getUserPresetFiles())
            if (file.getFileNameWithoutExtension() == smokePresetName)
                processor.loadUserPreset(file);
        check(processor.getPresetName() == smokePresetName, "user preset loaded");
        for (const auto& file : processor.getUserPresetFiles())
            if (file.getFileNameWithoutExtension() == smokePresetName)
                file.deleteFile();
    }

    if (failures == 0)
        std::printf("UI SMOKE TEST PASSED\n");
    else
        std::printf("UI SMOKE TEST: %d FAILURE(S)\n", failures);

    return selfTest ? (failures == 0 ? 0 : 1) : 0;
}
