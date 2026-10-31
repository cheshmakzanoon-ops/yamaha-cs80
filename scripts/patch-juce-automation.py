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


def main() -> int:
    if len(sys.argv) != 3:
        raise RuntimeError("usage: patch-juce-automation.py INPUT OUTPUT")
    original, output = map(Path, sys.argv[1:])
    expected = {
        "juce_audio_plugin_client_VST3.cpp": "adf5c18774c42bfab7d50478525f84dcab43e12ff8d9879749137d80c61336f0",
    }
    source = original.read_text(encoding="utf-8")
    actual = hashlib.sha256(source.encode("utf-8")).hexdigest()
    if expected.get(original.name) != actual:
        raise RuntimeError(f"Unreviewed JUCE automation wrapper source {original}: {actual}; expected {expected.get(original.name)}")
    patched = patch_vst3(source)
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
