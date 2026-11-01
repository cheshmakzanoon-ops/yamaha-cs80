# ODDSound MTS-ESP client

Vendored on 2026-10-31 from [ODDSound/MTS-ESP](https://github.com/ODDSound/MTS-ESP),
commit [`f214739b8832e7f297cb9970d0c0efbf783f1462`](https://github.com/ODDSound/MTS-ESP/commit/f214739b8832e7f297cb9970d0c0efbf783f1462).
The three upstream files below are byte-for-byte copies, without local edits.

The upstream licence is **BSD Zero Clause (SPDX: 0BSD)**, reproduced in
[LICENSE](LICENSE) and in both source files. It is not MIT. The surrounding
CS-80 Reference project remains AGPLv3.

| Upstream path | SHA-256 |
| --- | --- |
| `Client/libMTSClient.h` | `5c89f524b41f732b955e092c35522eb007dcb3a23db7631e9f9f75f4ffcf958f` |
| `Client/libMTSClient.cpp` | `b854e7c2b657a660431839fb1c413522fa2877296404cee2a36db55d8813c4b3` |
| `LICENSE` | `481d4c320dc5c1a7f5b4604c8e9a08ac7710e73291b177c468ab69e0fe158837` |

Only the client is vendored. A master and its platform `libMTS` installation
are still needed for a live MTS-ESP connection. The unmodified client discovers
the shared library at the upstream documented platform location; this project
does not install a replacement library, alter system paths, or supply a master.

`src/plugin/MtsEspClient.cpp` compiles the client into the existing adapter.
The adapter polls a channel-independent table in tuning mode 2; per-channel
tables and scale-name display remain outside this vendoring change. Tuning
mode 0 stays neutral, and PURE CS-80 ignores all tuning tables.

The core build compiles and runs the real client (`cs80_mts_esp_native_tests`),
a deterministic upstream-API fixture through the real adapter
(`cs80_mts_esp_shim_tests`), and the explicit no-client fallback
(`cs80_mts_esp_fallback_tests`). The fixture is compiled only into its test
executable. No replacement backend is present in the shipping plugin.


## Qualification

Acceptance was committed first in `tests/parity/tolerances-v2/mts-client.json`
(local commit `e38534502f101e3cc96d7bda13495fc975771b6a`). At this source pin:

- The native client registers and destroys 32 adapter instances without a master,
  preserves the caller's table, and parses an MTS single-note change from note
  69 to the frequency of note 70. The installed-master path is not exercised by
  this standalone process.
- The deterministic C API fixture supplies all 128 frequencies at +0.25
  semitones, rejects note 60, checks channel -1, verifies MIDI forwarding and
  null-input handling, and balances client registration and deregistration.
- Engine renders with that table are byte-identical at tuning mode 0 and in
  PURE CS-80 at tuning mode 2. Modern mode 2 changes the render, so these
  invariance checks cannot pass merely because the fixture is unused.
- The explicit `CS80_WITH_MTS_ESP=0` target compiles the fallback adapter too.
- Nine negative controls failed as required: an unavailable shipping client,
  dropped registration, dropped deregistration, dropped mapping, incorrect
  pitch conversion, dropped MIDI forwarding, wrong channel, removed Pure
  tuning mask, and removed default-mode tuning guard. The last two compiled
  mutated copies of the actual Engine implementation, not modified assertions.

No pitch-law tolerance was relaxed. The tests describe software behavior and
make no claim about physical CS-80 measurements.

### Native platform qualification, 2026-11-01

The real-client, shim, and no-client CTests all pass in the Linux, Windows,
macOS Apple Silicon, and macOS Intel jobs of
[build run 41251946760](https://github.com/cheshmakzanoon-ops/yamaha-cs80/actions/runs/41251946760).
The tested PR head is `156d82e9f3ee20089bb952ba853e0f0c779b903f`; the
adapter, vendored files, and three test sources are unchanged at the reviewed
head `475063c`. The
[feature evidence](../../docs/evidence/qualification-2026-11-01/host-features/README.md)
records the exact checkout, job results, log hashes, and source hashes.
The full Intel job failed a separate BBD test, so this evidence qualifies the
MTS paths without calling that complete workflow green. A live external
MTS-ESP master remains outside the standalone client fixture.
