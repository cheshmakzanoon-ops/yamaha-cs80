# MTS-ESP processor integration fixture

The dormant master-table mirror previously had source review but no direct
processor-level runtime witness. `cs80_mts_esp_processor_tests` now runs the real
`Cs80AudioProcessor::processBlock` and unchanged `MtsEspClient` adapter against a
deterministic external C API implementation in the test executable.

The existing `CS80_MTS_ESP_TEST_BACKEND` compile-time seam is target-private. It
omits only the upstream C API implementation in that executable's adapter object.
The executable resolves those symbols before the shipping static library, so the
library's normal adapter object is not pulled into this test. No test backend or
production source modification ships in the plugin.

## Matrix and controlled negative

Eight combinations cover Legacy/Circuit, initially eight voices or a prior
sixteen-to-eight transition, and zero/64-frame control blocks. Each fixture:

- Accepts a master table while the extension engine is dormant, including a filtered key.
- Rejects an invalid mapped frequency without partially replacing either engine's table.
- Accepts a later valid table, observes master disconnection, and enables sixteen voices.
- Checks both engines' tables on all sixteen channels and sounds all sixteen voices
  at the retained master pitch after disconnection.
- Verifies registration/deregistration and the channel-independent query convention.

With only the dormant master-table mirror temporarily removed, this exact fixture
reported **4,009 checks / 704 failures**. After restoration it reported
**4,009 checks / zero failures**. This negative control demonstrates regression
sensitivity for the already-fixed path, not a newly discovered production bug.

The restored fixture and actual-module VST3 automation test passed **2/2 in
4.61 seconds**. Production source diff is empty. The rebuilt shipping module's
SHA-256 remained `3aebf4640edb0b20ecc54c43f2b3cc11c80bef3387d6e76fdaaf37b741b81856`.

## Limits

This qualifies processor integration with a mocked external C API, not a live
MTS-ESP master, upstream shared-library discovery, service installation, concurrent
master changes, reconnect races, external DAWs, GUI, sanitizers or performance.
It does not claim the complete processor test suite passed. The fixture uses
48 kHz and 32-bit process buffers. No display interaction or audio device is needed.
