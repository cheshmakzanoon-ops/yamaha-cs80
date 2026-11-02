# Factory-tone state locale follow-up (2026-11-02)

Base implementation: `9ba7890543058a1de160b4181cbc413c9d2f5eac`, source
`2425af2dc94923e01193e864f110520a65d2ad5e`. Its C-locale processor pass is not
full persistence acceptance. The separate actual processor probe saves XML
`factory_tone_revision="1.0"`: C restores V2, but German and French comma
locales restore V1 (3 checks, 2 ordinary failures, exit1).

Before changing the decoder, add a required C/German processor-only CTest.
It uses actual session/user-preset/A-B serialization, both choices, actual
source-engine readback, missing/malformed data, the four original positive
literals, and deliberately registered decimal/exponent/hexadecimal forms.
Every restore must leave the caller's numeric locale unchanged. No display,
audio device, editor or DAW is created. Missing required locales fail.

The narrow repair may use a private explicit C-numeric locale with platform
`strtod_l` / `_strtod_l`, retaining complete consumption and exact0/1 meaning.
Allocation failure must safely fall back to V1; the locale must be released
with RAII. Production code must not call setlocale/uselocale or otherwise
change the host's global/thread locale. Native Windows/macOS compilation is
still required; Linux evidence does not establish those outcomes.

Registered negative: restore plain std::strtod in the actual production
decoder; actual comma-locale persistence must fail for its intended reason.
The unchanged pre-existing ENGINE/PURE decoder is a separate investigation;
it is not silently repaired by this follow-up.
