#pragma once
// Native built-module transport qualification. Controllers use IDs obtained
// from IMidiMapping, not guessed JUCE IDs, APVTS, or direct Engine injection.
// VST3 gives controllers and notes separate lists. All ordering assertions here
// use distinct offsets; no arbitrary MIDI wire ordering is claimed.
namespace {
struct MpeControl { int sample, channel, controller, value; };
using MpeControls = std::vector<MpeControl>;
constexpr int mpeFrames = 1536, mpeOnset = 128, mpeChange = 513;

void nativeRpn(MpeControls& controls, int sample, int channel, int rpn, int value)
{
    controls.push_back({sample, channel, 101, 0});
    controls.push_back({sample + 1, channel, 100, rpn});
    controls.push_back({sample + 2, channel, 6, value});
}
cs80::ParamValues mpeParameters(bool circuit, bool sixteen)
{
    auto p = cs80::defaultValues();
    p[cs80::P_sourcePerformance] = circuit ? 1.0f : 0.0f;
    for (int key : std::array<int, 9>{cs80::P_parityMode, cs80::P_drift, cs80::P_voiceDispersion,
                    cs80::P_analogNoise, cs80::P_detune, cs80::P_kasScanTiming,
                    cs80::P_arpOn, cs80::P_pressureCurve, cs80::P_pressureThreshold}) p[key] = 0;
    p[cs80::P_mpeOn] = 1;
    p[cs80::P_voiceMode] = sixteen ? 1.0f : 0.0f;
    // Deliberately disagree with MCM's negotiated 2/48 defaults.
    p[cs80::P_pitchBendRange] = 12;
    p[cs80::P_mpeBendRange] = 7;
    for (int rank = 0; rank < 2; ++rank)
    {
        p[cs80::chParam(rank, cs80::CP_afterLevel)] = 0.8f;
        p[cs80::chParam(rank, cs80::CP_afterBrill)] = 0.7f;
        p[cs80::chParam(rank, cs80::CP_lpfCutoff)] = 1200;
    }
    return p;
}

std::vector<float> renderNativeMpe(Module& module, const cs80::ParamValues& p,
    int noteChannel, MpeControls controls, int block = 256, bool reverseQueues = false,
    bool configureZones = true, bool witness = true)
{
    Instance instance(module, p);
    connectController(instance);
    if (configureZones)
    {
        nativeRpn(controls, 4, 1, 6, 2);
        nativeRpn(controls, 8, 16, 6, 2);
    }
    std::stable_sort(controls.begin(), controls.end(),
        [](const MpeControl& a, const MpeControl& b) { return a.sample < b.sample; });
    std::vector<float> output;
    for (int start = 0; start < mpeFrames; start += block)
    {
        const int count = std::min(block, mpeFrames - start);
        Changes changes;
        for (const auto& control : controls)
            if (control.sample >= start && control.sample < start + count)
            {
                const auto id = midiParameter(instance, control.controller, control.channel - 1);
                auto queue = std::find_if(changes.queues.begin(), changes.queues.end(),
                    [id](const auto& q) { return q->id == id; });
                // Pinned JUCE converts floor(v*128), or floor(v*16384) for bend.
                // Exact dyadic inputs reproduce the intended MIDI integer.
                const double value = double(control.value)
                    / (control.controller == kPitchBend ? 16384.0 : 128.0);
                if (queue == changes.queues.end()) changes.add(id, {{control.sample - start, value}});
                else (*queue)->points.emplace_back(control.sample - start, value);
            }
        if (reverseQueues) std::reverse(changes.queues.begin(), changes.queues.end());
        Events events;
        const auto note = [&](int sample, int channel, int pitch) {
            if (sample < start || sample >= start + count) return;
            Event event {};
            event.type = Event::kNoteOnEvent;
            event.sampleOffset = sample - start;
            event.noteOn.channel = static_cast<int16>(channel - 1);
            event.noteOn.pitch = static_cast<int16>(pitch);
            event.noteOn.velocity = 0.75f;
            event.noteOn.noteId = channel * 128 + pitch;
            events.events.push_back(event);
        };
        // The ninth held note exercises the second instrument in 16-voice mode.
        // Every comparison keeps these eight earlier onsets/allocations equal.
        if (p[cs80::P_voiceMode] > 0.5f)
            for (int key = 0; key < 8; ++key) note(32 + key, 8, 40 + key);
        if (witness) note(mpeOnset, noteChannel, 60);
        std::vector<float> left(static_cast<size_t>(count)), right(static_cast<size_t>(count));
        float* channels[] {left.data(), right.data()};
        AudioBusBuffers bus {}; bus.numChannels = 2; bus.channelBuffers32 = channels;
        ProcessContext context {}; context.sampleRate = testSampleRate; context.projectTimeSamples = start;
        ProcessData data {}; data.processMode = testProcessMode; data.symbolicSampleSize = kSample32;
        data.numSamples = count; data.numOutputs = 1; data.outputs = &bus;
        data.inputEvents = &events; data.inputParameterChanges = &changes; data.processContext = &context;
        require(instance.audio->process(data) == kResultOk, "native MPE process");
        for (int sample = 0; sample < count; ++sample)
        {
            require(std::isfinite(left[static_cast<size_t>(sample)]) && std::isfinite(right[static_cast<size_t>(sample)]),
                    "finite native MPE audio");
            output.push_back(left[static_cast<size_t>(sample)]);
            output.push_back(right[static_cast<size_t>(sample)]);
        }
    }
    return output;
}

void testVst3MpeTransport(Module& module)
{
    const int initialFailures = failures;
    std::string context = "mapping";
    const auto mpeCheck = [&](bool pass, const char* label) {
        check(pass, label);
        if (!pass) std::cerr << "  Native MPE context: " << context << '\n';
    };
    int scopeCases = 0, onsetCases = 0, boundaryCases = 0, rangeCases = 0;
    double leastPositive = 1.0, greatestIsolation = 0, greatestOrdering = 0;
    {
        Instance instance(module, mpeParameters(true, false));
        connectController(instance);
        std::vector<ParamID> ids, ordinary;
        std::vector<ParamID> exposed;
        for (int32 index = 0; index < instance.controller->getParameterCount(); ++index)
        {
            ParameterInfo info {};
            require(instance.controller->getParameterInfo(index, info) == kResultOk, "native MPE parameter metadata");
            exposed.push_back(info.id);
        }
        std::sort(exposed.begin(), exposed.end());
        mpeCheck(std::adjacent_find(exposed.begin(), exposed.end()) == exposed.end(),
              "native controller metadata contains no duplicate parameter IDs");
        for (int key = 0; key < cs80::P_numParams; ++key) ordinary.push_back(parameterId(cs80::paramId(key)));
        for (int rank = 1; rank <= 2; ++rank) ordinary.push_back(parameterId("hardware_source_" + std::to_string(rank)));
        for (int channel = 0; channel < 16; ++channel)
            for (int controller : {int(kPitchBend), int(kAfterTouch), 74, 101, 100, 6, 38})
            {
                const auto id = midiParameter(instance, controller, channel);
                mpeCheck(std::binary_search(exposed.begin(), exposed.end(), id), "native MPE mapped ID is exposed by the controller");
                mpeCheck(std::find(ids.begin(), ids.end(), id) == ids.end(), "native MPE IDs are disjoint across channel/controller pairs");
                mpeCheck(std::find(ordinary.begin(), ordinary.end(), id) == ordinary.end(), "native MPE IDs do not alias core or hardware-source parameter IDs");
                ids.push_back(id);
            }
        std::cout << "Native MPE mapping: " << ids.size() << " distinct channel/controller IDs\n";
    }
    const auto positive = [&](const std::vector<float>& a, const std::vector<float>& b, const char* label) {
        const double delta = difference(a, b);
        leastPositive = std::min(leastPositive, delta);
        mpeCheck(delta > 1.0e-6, label);
    };
    const auto isolated = [&](const std::vector<float>& a, const std::vector<float>& b, const char* label) {
        greatestIsolation = std::max(greatestIsolation, difference(a, b));
        mpeCheck(a == b, label);
    };
    for (bool circuit : {false, true}) for (bool sixteen : {false, true})
    {
        const auto p = mpeParameters(circuit, sixteen);
        const std::string policy = std::string(circuit ? "circuit" : "legacy")
            + (sixteen ? "/16" : "/8");
        context = policy + "/witness";
        // A separate positive witness prevents silent/unallocated ninth-note controls.
        positive(renderNativeMpe(module, p, 2, {}), renderNativeMpe(module, p, 2, {}, 256, false, true, false),
                 "native MPE witness note is audible with eight or sixteen voices");
        for (bool upper : {false, true}) for (bool manager : {false, true})
            for (int controller : {int(kPitchBend), int(kAfterTouch), 74})
            {
                context = policy + (upper ? "/upper" : "/lower") + (manager ? "/manager/" : "/member/")
                    + std::to_string(controller);
                const int member = upper ? 15 : 2, sibling = upper ? 14 : 3;
                const int address = manager ? (upper ? 16 : 1) : member;
                const MpeControls controls {{mpeChange, address, controller, controller == kPitchBend ? 10240 : 100}};
                const auto actual = renderNativeMpe(module, p, member, controls);
                const auto neutral = renderNativeMpe(module, p, member, {});
                positive(actual, neutral, "native MPE addressed expression changes audio");
                mpeCheck(std::equal(actual.begin(), actual.begin() + 2 * mpeChange, neutral.begin()),
                      "native MPE expression cannot affect samples before its offset");
                const auto siblingActual = renderNativeMpe(module, p, sibling, controls);
                const auto siblingNeutral = renderNativeMpe(module, p, sibling, {});
                if (manager) positive(siblingActual, siblingNeutral, "native MPE manager reaches another member in its zone");
                else isolated(siblingActual, siblingNeutral, "native MPE member expression preserves another member");
                for (int unrelated : {upper ? 2 : 15, 8})
                    isolated(renderNativeMpe(module, p, unrelated, controls), renderNativeMpe(module, p, unrelated, {}),
                             "native MPE expression preserves opposite zone and gap channel");
                ++scopeCases;
            }
        for (bool upper : {false, true}) for (int controller : {int(kPitchBend), int(kAfterTouch), 74})
        {
            context = policy + (upper ? "/upper/onset/" : "/lower/onset/") + std::to_string(controller);
            const int member = upper ? 15 : 2;
            const int value = controller == kPitchBend ? 10240 : 100;
            const auto early = renderNativeMpe(module, p, member, {{64, member, controller, value}});
            isolated(early, renderNativeMpe(module, p, member, {{mpeOnset - 1, member, controller, value}}),
                     "native pre-onset member expression is retained until its note");
            positive(early, renderNativeMpe(module, p, member, {}),
                     "native member expression received before onset changes its future note");
            ++onsetCases;
        }
        // Distinct offsets preserve chronological RPN and expression routing even
        // if the host enumerates parameter queues in the opposite order.
        context = policy + "/ordering";
        MpeControls ordered {{64, 2, kPitchBend, 9216}, {65, 2, kAfterTouch, 80}, {66, 2, 74, 90},
                             {mpeChange, 1, kPitchBend, 10240}, {mpeChange + 1, 1, kAfterTouch, 100},
                             {mpeChange + 2, 1, 74, 100}, {1025, 2, kPitchBend, 8192},
                             {1026, 2, kAfterTouch, 0}, {1027, 2, 74, 0}};
        const auto chronological = renderNativeMpe(module, p, 2, ordered);
        for (int block : {7, 256, 1536})
        {
            const auto reverse = renderNativeMpe(module, p, 2, ordered, block, true);
            greatestOrdering = std::max(greatestOrdering, difference(chronological, reverse));
            mpeCheck(chronological == reverse, "native MPE distinct-offset trace survives queue enumeration and block partitions");
        }
        positive(chronological, renderNativeMpe(module, p, 2, {}), "native MPE pre-onset and held expression trace is audible");
        for (int controller : {int(kPitchBend), int(kAfterTouch), 74})
        {
            context = policy + "/boundaries/" + std::to_string(controller);
            auto pure = p; pure[cs80::P_parityMode] = 1;
            const MpeControls control {{mpeChange, 2, controller, controller == kPitchBend ? 10240 : 100}};
            const auto withMpe = renderNativeMpe(module, pure, 2, control);
            pure[cs80::P_mpeOn] = 0;
            isolated(withMpe, renderNativeMpe(module, pure, 2, control), "PURE masks MPE policy at the native wrapper boundary");
            if (controller != kAfterTouch)
                isolated(withMpe, renderNativeMpe(module, pure, 2, {}), "PURE retains pitch-bend and CC74 exclusion");
            auto off = p; off[cs80::P_mpeOn] = 0;
            const auto noMpe = renderNativeMpe(module, off, 15, control);
            isolated(noMpe, renderNativeMpe(module, off, 15, control, 256, false, false),
                     "MPE-off ignores MCM setup without altering conventional expression");
            if (controller == kPitchBend)
            {
                positive(noMpe, renderNativeMpe(module, off, 15, {}), "MPE-off retains conventional global bend");
                isolated(noMpe, renderNativeMpe(module, off, 15, {{mpeChange, 1, controller, 10240}}),
                         "MPE-off bend is not falsely claimed to be member-scoped");
            }
            else isolated(noMpe, renderNativeMpe(module, off, 15, {}), "MPE-off non-manager pressure and CC74 stay on their channel");
            ++boundaryCases;
        }
        for (bool upper : {false, true}) for (bool manager : {false, true})
        {
            context = policy + (upper ? "/upper/range" : "/lower/range") + (manager ? "/manager" : "/member");
            const int member = upper ? 15 : 2, address = manager ? (upper ? 16 : 1) : member;
            const MpeControls bend {{mpeChange, address, kPitchBend, 10240}};
            auto fallback = p; fallback[cs80::P_pitchBendRange] = 2; fallback[cs80::P_mpeBendRange] = 48;
            const auto configured = renderNativeMpe(module, p, member, bend);
            isolated(configured, renderNativeMpe(module, fallback, member, bend),
                     "native MCM 2/48 defaults override deliberately different fallback knobs");
            auto explicitDefault = bend;
            nativeRpn(explicitDefault, 20, address, 0, manager ? 2 : 48);
            isolated(configured, renderNativeMpe(module, p, member, explicitDefault),
                     "native MCM defaults match independently transmitted RPN0 2/48 sensitivity");
            auto negotiated = bend;
            // Choose dyadic wheel fractions whose effective semitones also have
            // an exact independent oracle under MCM's 2/48 ranges.
            negotiated.front().value = manager ? 9216 : 11264;
            nativeRpn(negotiated, 20, address, 0, manager ? 12 : 24);
            // Keep the RPN selected so CC38 independently exercises the fine part.
            negotiated.push_back({23, address, 38, 50});
            const auto ranged = renderNativeMpe(module, p, member, negotiated);
            isolated(ranged, renderNativeMpe(module, fallback, member, negotiated),
                     "native RPN0 negotiated sensitivity overrides fallback knobs");
            const MpeControls equivalent {{mpeChange, address, kPitchBend, manager ? 14592 : 9760}};
            isolated(ranged, renderNativeMpe(module, p, member, equivalent),
                     "native RPN0 12.5/24.5 bend matches exact 2/48-range semitone oracle");
            positive(ranged, configured, "native RPN0 coarse/fine range changes audible bend after MCM");
            ++rangeCases;
        }
    }
    std::cout << "Native MPE transport: scope_cases=" << scopeCases << " onset_cases=" << onsetCases
              << " boundary_cases=" << boundaryCases
              << " range_cases=" << rangeCases << " least_positive_delta=" << leastPositive
              << " max_isolation_error=" << greatestIsolation << " max_ordering_error=" << greatestOrdering
              << " failures=" << failures - initialFailures << '\n';
}
} // namespace
