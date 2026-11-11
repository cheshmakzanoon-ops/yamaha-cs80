#pragma once
// Session qualification through the built module's IComponent and native
// IBStream protocol. No AudioProcessor, APVTS, or tuning parser is linked here.
namespace {
struct ScalaNativeState
{
    std::string xml, trailer;
};
ScalaNativeState splitScalaNativeState(const std::string& bytes)
{
    // JUCE 8.0.11, JUCE_VST3_CAN_REPLACE_VST2=0: processor XML followed by
    // eight zero bytes, private data, its LE uint64 size, and this magic.
    const std::string magic = "JUCEPrivateData";
    require(bytes.size() >= magic.size() + 16
        && bytes.compare(bytes.size() - magic.size(), magic.size(), magic) == 0,
        "native Scala state retains the JUCE wrapper trailer");
    const auto sizeOffset = bytes.size() - magic.size() - 8;
    std::uint64_t privateSize = 0;
    for (unsigned i = 0; i < 8; ++i)
        privateSize |= std::uint64_t(static_cast<unsigned char>(bytes[sizeOffset + i])) << (8 * i);
    require(privateSize <= sizeOffset - 8, "bounded native private-state payload");
    const auto xmlSize = sizeOffset - static_cast<size_t>(privateSize) - 8;
    require(bytes.substr(xmlSize, 8) == std::string(8, '\0'), "native state XML/trailer separator");
    ScalaNativeState state {bytes.substr(0, xmlSize), bytes.substr(xmlSize)};
    require(state.xml.find("<CS80_PARAMS ") != std::string::npos
        && state.xml.find("</CS80_PARAMS>") != std::string::npos,
        "native state contains the processor session XML");
    return state;
}
std::string captureScalaState(Instance& instance)
{
    StateCapture stream;
    require(instance.component->getState(&stream) == kResultOk, "IComponent getState for Scala session");
    splitScalaNativeState(stream.bytes);
    return stream.bytes;
}
void restoreScalaState(Instance& instance, const std::string& bytes)
{
    splitScalaNativeState(bytes);
    StateCapture stream;
    stream.bytes = bytes;
    require(stream.seek(0, IBStream::kIBSeekSet, nullptr) == kResultOk, "rewind Scala IBStream");
    // The wrapper accepts the stream even when the processor rejects malformed
    // embedded tuning. Verify rejection via saved state and audio, not this code.
    require(instance.component->setState(&stream) == kResultOk, "IComponent setState for Scala session");
}
std::string scalaXmlAttribute(const std::string& text)
{
    std::string result;
    for (const auto c : text)
        switch (c)
        {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\n': result += "&#10;"; break;
            case '\r': result += "&#13;"; break;
            case '\t': result += "&#9;"; break;
            default: result += c; break;
        }
    return result;
}
std::string scalaStateWithTuning(const std::string& bytes, const std::string& scl, const std::string& kbm)
{
    auto state = splitScalaNativeState(bytes);
    const auto begin = state.xml.find("<WS3_TUNING ");
    if (begin != std::string::npos)
    {
        const auto end = state.xml.find("/>", begin);
        require(end != std::string::npos, "complete native tuning element");
        state.xml.erase(begin, end + 2 - begin);
    }
    if (!scl.empty())
        state.xml.insert(state.xml.find("</CS80_PARAMS>"), "<WS3_TUNING scl=\""
            + scalaXmlAttribute(scl) + "\" kbm=\"" + scalaXmlAttribute(kbm) + "\"/>");
    return state.xml + state.trailer;
}
bool scalaTextsEqual(const std::string& bytes, const std::string& scl, const std::string& kbm)
{
    const auto state = splitScalaNativeState(bytes);
    const auto begin = state.xml.find("<WS3_TUNING ");
    if (begin == std::string::npos) return scl.empty() && kbm.empty();
    const auto end = state.xml.find("/>", begin);
    require(end != std::string::npos, "serialized native tuning element");
    const auto element = state.xml.substr(begin, end + 2 - begin);
    return element.find("scl=\"" + scalaXmlAttribute(scl) + "\"") != std::string::npos
        && element.find("kbm=\"" + scalaXmlAttribute(kbm) + "\"") != std::string::npos;
}
const std::string nativeScalaScale = "Native nonuniform & signed\n5\n100.0\n300.0\n500.0\n800.0\n1200.0\n";
const std::string nativeScalaMap = "3\n36\n91\n60\n60\n440.0\n3\n-1\n0\nx\n";
const std::vector<int> nativeScalaNotes {42,43,45,46,48,49,51,52,54,55,57,58,60,61,63,64};
cs80::ParamValues scalaParameters(bool circuit, bool sixteen)
{
    auto p = cs80::defaultValues();
    p[cs80::P_sourcePerformance] = circuit ? 1.f : 0.f;
    p[cs80::P_parityMode] = 0; p[cs80::P_tuningMode] = 1;
    p[cs80::P_voiceMode] = sixteen ? 1.f : 0.f;
    p[cs80::P_arpOn] = p[cs80::P_mpeOn] = p[cs80::P_kasScanTiming] = 0;
    p[cs80::P_drift] = p[cs80::P_voiceDispersion] = p[cs80::P_analogNoise] = p[cs80::P_detune] = 0;
    return p;
}
std::vector<float> renderScalaNotes(Instance& instance, const std::vector<int>& notes, int frames = 2048)
{
    require(frames > 0 && frames % 256 == 0 && notes.size() <= 256, "bounded native Scala render fixture");
    std::vector<float> output;
    for (int start = 0; start < frames; start += 256)
    {
        std::array<float,256> left {}, right {};
        float* channels[] {left.data(), right.data()};
        AudioBusBuffers bus {}; bus.numChannels = 2; bus.channelBuffers32 = channels;
        Events events;
        if (start == 0)
            for (size_t i = 0; i < notes.size(); ++i)
            {
                Event e {}; e.type = Event::kNoteOnEvent; e.sampleOffset = static_cast<int32>(i);
                e.noteOn.channel = 0; e.noteOn.pitch = static_cast<int16>(notes[i]);
                e.noteOn.noteId = static_cast<int32>(i); e.noteOn.velocity = .75f;
                events.events.push_back(e);
            }
        ProcessContext context {}; context.sampleRate = testSampleRate; context.projectTimeSamples = start;
        ProcessData data {}; data.processMode = testProcessMode; data.symbolicSampleSize = kSample32;
        data.numSamples = 256; data.numOutputs = 1; data.outputs = &bus;
        data.inputEvents = &events; data.processContext = &context;
        require(instance.audio->process(data) == kResultOk, "native Scala note processing");
        for (int i = 0; i < 256; ++i) { output.push_back(left[i]); output.push_back(right[i]); }
    }
    return output;
}
std::vector<float> renderScalaState(Module& module, const cs80::ParamValues& p,
                                    const std::string& state, const std::vector<int>& notes)
{
    Instance instance(module, p);
    restoreScalaState(instance, state);
    return renderScalaNotes(instance, notes);
}
bool scalaSilent(const std::vector<float>& output)
{
    return std::all_of(output.begin(), output.end(), [](float sample) { return sample == 0.f; });
}
void testVst3ScalaSessions(Module& module, double audioTolerance)
{
    const auto initialFailures = failures;
    int cases = 0;
    double roundtripError = 0, activationError = 0;
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    {
        const auto p = scalaParameters(circuit, sixteen);
        Instance donor(module, p);
        const auto absent = captureScalaState(donor);
        const auto incoming = scalaStateWithTuning(absent, nativeScalaScale, nativeScalaMap);
        restoreScalaState(donor, incoming);
        check(scalaTextsEqual(captureScalaState(donor), nativeScalaScale, nativeScalaMap),
              "native restore accepts exact nonuniform SCL and signed/unmapped KBM texts");
        auto notes = nativeScalaNotes;
        if (!sixteen) notes.resize(8);
        const auto reference = renderScalaNotes(donor, notes); // Render before saving.
        check(!scalaSilent(reference), "native Scala pre-save reference is audible");
        const auto saved = captureScalaState(donor);
        Instance restored(module, scalaParameters(!circuit, !sixteen));
        restoreScalaState(restored, saved);
        const auto restoredState = captureScalaState(restored);
        check(scalaTextsEqual(restoredState, nativeScalaScale, nativeScalaMap),
              "fresh IComponent session retains exact embedded SCL/KBM");
        check(restoredState == saved, "fresh IComponent getState roundtrip preserves the full native session");
        const auto error = difference(reference, renderScalaNotes(restored, notes));
        roundtripError = std::max(roundtripError, error);
        check(error <= audioTolerance, "fresh native component reproduces pre-save Scala audio");
        check(difference(reference, renderScalaState(module, p, absent, notes)) > 1e-6,
              "embedded Scala changes native output versus absent tuning");
        const auto unsignedMap = "3\n36\n91\n60\n60\n440.0\n3\n0\n1\nx\n";
        check(difference(reference, renderScalaState(module, p,
              scalaStateWithTuning(absent, nativeScalaScale, unsignedMap), notes)) > 1e-6,
              "negative KBM degree is an audible part of the native fixture");
        // x entries within the playable range, plus both range exclusions.
        check(scalaSilent(renderScalaState(module, p, saved, {38,41,44,47,50,53,56,59,62,65,68,71,35,92,0,127})),
              "fresh restored native session keeps unmapped/out-of-range notes silent");
        Instance cleared(module, p);
        restoreScalaState(cleared, saved);
        restoreScalaState(cleared, absent);
        check(scalaTextsEqual(captureScalaState(cleared), "", ""), "absent tuning clears native saved Scala state");
        check(difference(renderScalaNotes(cleared, notes), renderScalaState(module, p, absent, notes)) <= audioTolerance,
              "absent tuning restores native equal-temperament audio");
        for (bool previousTuning : {false, true}) for (bool badMapping : {false, true})
        {
            auto retainedParameters = p;
            retainedParameters[cs80::P_volume] = .7f;
            Instance retained(module, retainedParameters, 2);
            if (previousTuning)
                restoreScalaState(retained, scalaStateWithTuning(captureScalaState(retained),
                    "Prior tuning\n1\n3/1\n", ""));
            const auto before = captureScalaState(retained);
            const auto invalid = scalaStateWithTuning(incoming,
                badMapping ? nativeScalaScale : "malformed scale", badMapping ? "malformed mapping" : nativeScalaMap);
            restoreScalaState(retained, invalid);
            check(captureScalaState(retained) == before,
                  "invalid native SCL/KBM rejects all session changes with/without prior tuning");
            check(difference(renderScalaNotes(retained, notes),
                  renderScalaState(module, retainedParameters, before, notes)) <= audioTolerance,
                  "invalid native tuning retains prior audible session");
            // A rejected load must not prevent the next valid one from committing.
            restoreScalaState(retained, saved);
            check(scalaTextsEqual(captureScalaState(retained), nativeScalaScale, nativeScalaMap),
                  "valid native tuning restore succeeds after rejected state");
        }
        ++cases;
    }
    for (bool circuit : {false, true})
    {
        const auto p = scalaParameters(circuit, false);
        Instance switched(module, p), lateRestore(module, p), eight(module, p);
        const auto state = scalaStateWithTuning(captureScalaState(switched), nativeScalaScale, nativeScalaMap);
        restoreScalaState(switched, state);
        restoreScalaState(eight, state);
        // Consume the tuning version in eight-voice mode before activating the
        // extension. A dormant extension must receive the saved tuning too.
        for (auto* instance : {&switched, &lateRestore, &eight}) renderScalaNotes(*instance, {}, 256);
        // The independent reference loads tuning only at activation. Both
        // extensions begin at the same frame; comparing with an already-running
        // sixteen-voice extension would conflate free-running oscillator phase.
        restoreScalaState(lateRestore, state);
        Changes activate; activate.add(parameterId(cs80::paramId(cs80::P_voiceMode)), {{0, 1.0}});
        switched.flush(activate);
        lateRestore.flush(activate);
        const auto actual = renderScalaNotes(switched, nativeScalaNotes);
        const auto error = difference(actual, renderScalaNotes(lateRestore, nativeScalaNotes));
        activationError = std::max(activationError, error);
        check(error <= audioTolerance, "8-to-16 native activation retains tuning in the newly active engine");
        check(difference(actual, renderScalaNotes(eight, nativeScalaNotes)) > 1e-6,
              "8-to-16 native activation changes the audible sixteen-note render");
        Instance silentSwitch(module, p);
        restoreScalaState(silentSwitch, state);
        renderScalaNotes(silentSwitch, {}, 256);
        silentSwitch.flush(activate);
        check(scalaSilent(renderScalaNotes(silentSwitch, {38,41,44,47,50,53,56,59,62,65,68,71,35,92,0,127})),
              "8-to-16 native activation preserves unmapped-note silence");
        ++cases;
    }
    std::cout << "Native Scala IComponent sessions: " << cases << " cases, roundtrip_error=" << roundtripError
              << " activation_error=" << activationError << " failures=" << failures - initialFailures << '\n';
}
} // namespace
