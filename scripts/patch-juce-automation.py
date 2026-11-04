#!/usr/bin/env python3
"""Generate the small host-queue bridge without changing a JUCE checkout.

The supported source is JUCE 8.0.11, c352e2489000ffeb9e397051377d75eef2efe641.
SHA-256 is over UTF-8 text with LF line endings, so Windows autocrlf does not
look like a source change. Every other source change fails closed for review.
"""
from pathlib import Path
import hashlib
import sys


def replace_once(text: str, before: str, after: str) -> str:
    if text.count(before) != 1:
        raise RuntimeError(f"JUCE bridge anchor is not unique: {before[:100]!r}")
    return text.replace(before, after)


def patch_vst3(source: str) -> str:
    source = replace_once(source,
        "#include <juce_audio_plugin_client/VST3/juce_VST3ModuleInfo.h>",
        '#include <juce_audio_plugin_client/VST3/juce_VST3ModuleInfo.h>\n#include "plugin/HostAutomationBridge.h"')
    source = replace_once(source,
        "void processParameterChanges (Vst::IParameterChanges& paramChanges)",
        "void processParameterChanges (Vst::IParameterChanges& paramChanges, cs80::HostAutomationClient* automation)")
    source = replace_once(source, """                if (const auto change = getPointFromQueue (paramQueue, numPoints - 1))
                {
                    if (auto* param = comPluginInstance->getParamForVSTParamID (vstParamID))
                        setValueAndNotifyIfChanged (*param, (float) change->value);
                }""", """                if (auto* param = comPluginInstance->getParamForVSTParamID (vstParamID))
                {
                    // CS-80 bridge: the host owns these points until process()
                    // returns. Keep every offset and value, with no RT copy.
                    const cs80::HostAutomationQueue queue {
                        paramQueue, numPoints,
                        [] (void* context, int index, int& offset, double& value) noexcept
                        {
                            Steinberg::int32 hostOffset = 0;
                            const auto result = static_cast<Vst::IParamValueQueue*> (context)
                                ->getPoint (index, hostOffset, value);
                            offset = hostOffset;
                            return result == kResultTrue;
                        },
                        param,
                        [] (void* parameter, float value) noexcept
                        {
                            setValueAndNotifyIfChanged (*static_cast<AudioProcessorParameter*> (parameter), value);
                        }
                    };
                    if (automation != nullptr && automation->attachHostAutomationQueue (*param, queue))
                        continue;
                    if (const auto change = getPointFromQueue (paramQueue, numPoints - 1))
                        setValueAndNotifyIfChanged (*param, (float) change->value);
                }""")
    source = replace_once(source, """        if (data.inputParameterChanges != nullptr)
            processParameterChanges (*data.inputParameterChanges);""", """        cs80::HostAutomationBlock cs80Automation {
            dynamic_cast<cs80::HostAutomationClient*> (pluginInstance), data.numSamples };
        if (data.inputParameterChanges != nullptr)
            processParameterChanges (*data.inputParameterChanges, cs80Automation.get());""")
    return source


def patch_au(source: str) -> str:
    source = replace_once(source,
        "#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>",
        '#include <juce_audio_plugin_client/detail/juce_PluginUtilities.h>\n#include "plugin/AuAutomationQueue.h"')
    source = replace_once(source,
        """    // No idea what this method actually does or what it should return. Current Apple docs say nothing about it.
    // (Note that this isn't marked 'override' in case older versions of the SDK don't include it)
    bool CanScheduleParameters() const override          { return false; }""",
        """    #include "plugin/AuAutomationHooks.inc"

    // Scheduled key controls use the CS-80 queue; other controls retain their
    // former immediate/block-rate behavior. Events use preallocated CS-80 storage.
    bool CanScheduleParameters() const override          { return true; }""")
    source = replace_once(source,
        """                if (! isParameterDiscrete)
                    outParameterInfo.flags |= kAudioUnitParameterFlag_CanRamp;""",
        """                if (! isParameterDiscrete && cs80AutomationIndex (*param) >= 0)
                    outParameterInfo.flags |= kAudioUnitParameterFlag_CanRamp;""")
    source = replace_once(source,
        """                const auto normValue = param->getValue();""",
        """                const auto normValue = cs80ReadAuParameter (*param);""")
    source = replace_once(source,
        """                auto value = inValue / getMaximumParameterValue (param);""",
        """                if (inBufferOffsetInFrames != 0 && cs80AutomationIndex (*param) >= 0)
                    if (auto* client = cs80AutomationClient(); client != nullptr && client->usesHostAutomationTiming())
                        return cs80ScheduleAuImmediate (*param, inValue, inBufferOffsetInFrames);

                cs80InvalidateAuParameter (*param);
                auto value = inValue / getMaximumParameterValue (param);""")
    source = replace_once(source,
        """        sendAUEvent (kAudioUnitEvent_ParameterValueChange, index);""",
        """        if (auto* parameter = juceParameters.getParamForIndex (index))
            cs80InvalidateAuParameter (*parameter);
        sendAUEvent (kAudioUnitEvent_ParameterValueChange, index);""")
    source = replace_once(source,
        """        lastTimeStamp = inTimeStamp;

        // prepare buffers""",
        """        lastTimeStamp = inTimeStamp;
        cs80::HostAutomationBlock cs80Automation { cs80AutomationClient(), static_cast<int> (nFrames) };
        if (! cs80PrepareAuAutomation (cs80Automation.get(), static_cast<int> (nFrames)))
            return kAudioUnitErr_InvalidParameter;

        // prepare buffers""")
    for signature in ["ComponentResult Initialize() override", "void Cleanup() override",
                      "ComponentResult Reset (AudioUnitScope inScope, AudioUnitElement inElement) override"]:
        source = replace_once(source, signature + "\n    {", signature
            + "\n    {\n        cs80AuAutomation.clearPending();\n        cs80ResetAuPublications();")
    source = replace_once(source,
        "ComponentResult RestoreState (CFPropertyListRef inData) override\n    {",
        "ComponentResult RestoreState (CFPropertyListRef inData) override\n    {\n        cs80ResetAuPublications();")
    return source


def reviewed_source(original: Path) -> str:
    expected = {
        "juce_audio_plugin_client_VST3.cpp": "adf5c18774c42bfab7d50478525f84dcab43e12ff8d9879749137d80c61336f0",
        "juce_audio_plugin_client_VST3.mm": "2f074fef9a5940c0f0e7aef15ca9ae024e44bf613ca1c6b828842b9282ebd532",
        "juce_audio_plugin_client_AU_1.mm": "a4a8f4efb3b20977a1dfa8865c02800834e8790043975428b28358417d1315f5",
    }
    source = original.read_text(encoding="utf-8")
    actual = hashlib.sha256(source.encode("utf-8")).hexdigest()
    if expected.get(original.name) != actual:
        raise RuntimeError(f"Unreviewed JUCE automation wrapper source {original}: {actual}; expected {expected.get(original.name)}")
    return source


def main() -> int:
    if len(sys.argv) != 3:
        raise RuntimeError("usage: patch-juce-automation.py INPUT OUTPUT")
    original, output = map(Path, sys.argv[1:])
    source = reviewed_source(original)
    if original.name == "juce_audio_plugin_client_VST3.mm":
        # The reviewed macOS translation unit only includes VST3.cpp. Expand
        # the separately reviewed implementation into the generated .mm so
        # Objective-C++ remains enabled and no pristine include bypasses the
        # queue bridge. Neither original source file is modified.
        source = reviewed_source(original.with_suffix(".cpp"))
    patched = patch_au(source) if original.name == "juce_audio_plugin_client_AU_1.mm" else patch_vst3(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.exists() or output.read_text(encoding="utf-8") != patched:
        output.write_text(patched, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
