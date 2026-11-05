# ODDSound MTS-ESP client

Source: [ODDSound/MTS-ESP](https://github.com/ODDSound/MTS-ESP), commit
[`f214739b8832e7f297cb9970d0c0efbf783f1462`](https://github.com/ODDSound/MTS-ESP/commit/f214739b8832e7f297cb9970d0c0efbf783f1462).
Retrieved and checked directly against that upstream commit on 2026-11-05.
All three files below are unmodified upstream bytes.

| File | SHA-256 |
| --- | --- |
| `Client/libMTSClient.h` | `5c89f524b41f732b955e092c35522eb007dcb3a23db7631e9f9f75f4ffcf958f` |
| `Client/libMTSClient.cpp` | `b854e7c2b657a660431839fb1c413522fa2877296404cee2a36db55d8813c4b3` |
| `LICENSE` | `481d4c320dc5c1a7f5b4604c8e9a08ac7710e73291b177c468ab69e0fe158837` |

The upstream licence is **BSD Zero Clause (SPDX: 0BSD)**, not MIT. The full
permission grant is reproduced in `LICENSE` and both source-file headers.
The surrounding CS-80 project remains AGPLv3.

## Integration boundary

Only the client is distributed. A master and the platform `libMTS` library
are still needed for live tuning. Their installation and system configuration
are not performed by this project. The upstream loader looks for:

- Windows: `MTS-ESP\LIBMTS.dll` under the common Program Files directory.
- macOS: `/Library/Application Support/MTS-ESP/libMTS.dylib`.
- Linux: `/usr/local/lib/libMTS.so`.

The adapter queries a channel-independent table (`channel = -1`) once per
audio block when tuning mode 2 is selected. It preserves the existing table
when no master is available, a mapped frequency is unsafe, or a disconnected
master is observed at the final check. Filtered notes retain a finite pitch. The API
does not offer an atomic snapshot of a master editing its table; the final
connection check detects a continuing disconnect, not rapid disconnect/reconnect
cycles or all concurrent tuning edits. Publication to the caller is all-or-nothing;
the external source snapshot is not guaranteed coherent.

The plugin's existing, separately tested `Engine::handleSysEx` parser remains
responsible for MIDI Tuning Standard SysEx. The adapter does **not** call the
vendored `MTS_ParseMIDIData(U)` functions or consume the client's local SysEx
table. This intentionally avoids activating a second parser with different
validation and decoding behavior. Previous experimental vendoring branches
modified the upstream two-byte scale decoder; that patch is unnecessary for
this live-master-only boundary and is not included here.

Per-channel master tables, scale-name display, master installation and live
external-master DAW qualification are outside this integration. Core tests
exercise the real client without a master, a deterministic C API fixture, the
explicit no-client build and the Windows max-macro compile boundary. These
are software tests, not physical CS-80 measurements.
