# Explicit Apple locale API includes (2026-11-02)

This separate platform-include amendment does not claim an observed Mac build
failure. The frozen Linux-approved feature and existing-policy tips remain
unchanged. Pinned JUCE8.0.11 atc352e2489000ffeb9e397051377d75eef2efe641 has
an explicit xlocale.h include under JUCE_MAC || JUCE_IOS in
modules/juce_core/system/juce_StandardHeader.h, lines106–111. The processor
includes JUCE audio modules through PluginProcessor.h, so the header was
already transitively reachable. Verified header SHA256: dc6e2401189c2eae507417e0bcbd3f1bb153ecb8555783c3f01c96c8f47fb50c.

Apple documentation lists both
[stdlib.h and xlocale.h for strtod_l](https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man3/strtod_l.3.html).
The processor now explicitly includes those headers on Apple only, avoiding
reliance on JUCE/libc++ transitive inclusion for its platform API declarations.
No parsing, lifetime, runtime locale, parameter, state or DSP semantics change.
Linux/Windows do not enter the Apple preprocessor branch.

Pinned JUCE source and primary declaration evidence are not native SDK
compilation. Fresh Apple Silicon/Intel native builds remain required and
publication-held; this amendment does not close that gate by inference.
