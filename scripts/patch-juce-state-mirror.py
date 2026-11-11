#!/usr/bin/env python3
"""Generate a narrowly reviewed JUCE 8.0.11 mirror bridge in the build tree."""
import hashlib
from pathlib import Path
import sys

PINS = {
    "juce_audio_processors.cpp": "f28f656122703bf353e2c74a4fbd2fbb392c22efe5d88e188df1b47ed56ca39d",
    "juce_audio_processors.mm": "89543472eb0763dbbdbcec446d5b6729fc1beac0a8e796aadf865d628566f8cc",
    "utilities/juce_AudioProcessorValueTreeState.cpp": "b3bd9f8f1670cf286ff7d558e45d2a79032a8888ee6083c106946d51c584fd9a",
}
ANCHOR = '''void AudioProcessorValueTreeState::valueTreePropertyChanged (ValueTree& tree, const Identifier&)
{
    if (tree.hasType (valueType) && tree.getParent() == state)
        setNewState (tree);
}'''
REPLACEMENT = '''void AudioProcessorValueTreeState::valueTreePropertyChanged (ValueTree& tree, const Identifier& property)
{
    if (cs80::ParameterStateMirrorScope::suppresses (this, tree, property))
        return;
    if (tree.hasType (valueType) && tree.getParent() == state)
        setNewState (tree);
}'''

def replace_once(text, old, new):
    if text.count(old) != 1:
        raise ValueError("review required: expected exactly one anchor: " + old[:80])
    return text.replace(old, new, 1)

def save(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    if not path.exists() or path.read_text(encoding="utf-8") != text:
        path.write_text(text, encoding="utf-8", newline="\n")

def generate(source, destination):
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if source.name not in ("juce_audio_processors.cpp", "juce_audio_processors.mm"):
        raise ValueError("review required: unknown module source")
    if source.suffix != destination.suffix:
        raise ValueError("review required: module language must be preserved")
    root = source.parent
    impl = destination.parent / "cs80_AudioProcessorValueTreeState.cpp"
    protected = {(root / name).resolve() for name in PINS}
    for output in (destination, impl.resolve()):
        if output.is_relative_to(root) or output in protected:
            raise ValueError("review required: outputs must not overwrite the pristine module")
    inputs = {}
    for name, digest in PINS.items():
        text = (root / name).read_text(encoding="utf-8")  # LF normalization supports Git CRLF checkouts.
        if hashlib.sha256(text.encode()).hexdigest() != digest:
            raise ValueError("review required: unrecognized JUCE source: " + name)
        inputs[name] = text
    impl_name = "utilities/juce_AudioProcessorValueTreeState.cpp"
    patched = replace_once(inputs[impl_name], ANCHOR, REPLACEMENT)
    patched = replace_once(patched, "namespace juce\n{", '#include "plugin/ParameterStateMirror.h"\n\nnamespace juce\n{')
    # Expand the reviewed cpp directly into .mm when JUCE selects Objective-C++.
    if source.suffix == ".mm":
        replace_once(inputs[source.name], '#include "juce_audio_processors.cpp"', "reviewed trampoline")
    module = replace_once(inputs["juce_audio_processors.cpp"], '#include "' + impl_name + '"',
                          '#include "' + impl.as_posix() + '"')
    save(impl, patched)
    save(destination, module)

if __name__ == "__main__":
    try:
        if len(sys.argv) != 3:
            raise ValueError("usage: patch-juce-state-mirror.py INPUT.cpp|mm OUTPUT.cpp|mm")
        generate(sys.argv[1], sys.argv[2])
    except (OSError, ValueError) as error:
        sys.exit(str(error))
