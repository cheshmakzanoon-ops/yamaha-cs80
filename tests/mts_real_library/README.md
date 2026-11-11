# Optional real MTS-ESP IPC qualification

Initial development runs passed on Linux x86_64, 2026-11-11, twice serially. An independent fresh-output replay of the final hardened harness passed all eight handshake stages, with distinct master/client process IDs, both exits 0, client count returning to 0 and no shared object remaining. No production-source changes or system installation. GCC 14.2.0, C++17, O2. Production repository revision: e4908e6e33e83732ac15524e8d305301c5bb2069.

## Reproduce

Keep prepare.py, build.sh, run.py, master.cpp and client.cpp together. Supply an existing official upstream checkout and this project's repository. An optional --library argument selects the exact official x86_64 binary; its SHA-256 is pinned and enforced. The evidence directory must be fresh and empty, and must not overlap the repository, upstream checkout or harness directory. All four upstream wrapper source/header hashes and the official binary hash are enforced. Python optimization (-O or PYTHONOPTIMIZE) is refused. The copied loader literal uses escaped C++ path bytes.

```sh
python prepare.py --repo /path/to/yamaha-cs80 --upstream /path/to/MTS-ESP --evidence /path/to/evidence
bash /path/to/evidence/build.sh > /path/to/evidence/build.log 2>&1
python -u /path/to/evidence/run.py > /path/to/evidence/run.log 2>&1
```

The attempted standard-path installation failed because this environment made `/usr/local/lib` read-only. No system library was installed. This adapted-loader harness requires no installation or administrator action. Never run while another MTS application is using or could start using this host's IPC namespace. Initial absence checks cannot lock out unrelated concurrent applications.

## Provenance and exact adaptation

Official upstream checkout: https://github.com/ODDSound/MTS-ESP, commit f214739b8832e7f297cb9970d0c0efbf783f1462. Binary: libMTS/Linux/x86_64/libMTS.so, SHA-256 03d23cb8d64ae246ceb416a7323cb230c5748c4b58d52a7c288d2c93ea3b7de5.

prepare.py copies the official Master and Client header/source pairs into the evidence directory. The only copied source alteration is replacing the single hardcoded /usr/local/lib/libMTS.so loader path with the absolute workspace path to that byte-identical official library. Master-loader.patch and Client-loader.patch record both diffs. The source-hashes.txt records production adapter/header/tuning and upstream/adapted inputs. The runner verifies these hashes again before starting either executable. build.sh also writes postbuild.sha256 for master.cpp, client.cpp, prepare.py, run.py, build.sh and both executables; run.py requires the complete manifest and verifies every entry before launching. Rebuild after changing any harness file.

The production src/plugin/MtsEspClient.cpp is compiled directly, unchanged. CS80_MTS_ESP_TEST_BACKEND=1 prevents its internal source inclusion, permitting separate linkage with the path-adapted official Client/libMTSClient.cpp. This is not a fake C API backend. The pinned in-tree client is byte-compared with the official upstream client during preparation.

## Scope covered

Two independent executables, driven by pipe handshakes. Every master mutation completes before its corresponding client reads. Master reports MTS_HasIPC=1 and zero initial clients; client-count checks observe 1 during qualification and 0 after client destruction.

- All 128 global pitches and all 16 x 128 channel pitches, with differing offsets. Pitch checks use an absolute tolerance below 0.00003 semitone; retained-table and independent-bank checks use exact array equality.
- Global filtering and channel-specific filtering.
- Channel-independent bank copies match the global table, followed immediately by channel-aware reads, covering upstream capability priming.
- Scale name crosses IPC and its newline is sanitized by the adapter.
- Subsequent whole-table update changes observed pitches.
- Late-note negative mapped frequency rejects global table and independent bank without changing any prior values.
- Invalid frequency at channel 15/note 127 rejects the entire channel bank without changing any prior values.
- Filtering each invalid key admits the update and stores a finite safe pitch.
- Master deregistration is observed by the same living client: empty name, failed reads, unchanged tables.
- Re-registration by the same master process is observed by the same client, with new tuning.

## Safety and cleanup

Before either process starts, run.py refuses pre-existing /dev/shm/libMTSShared, both possible configuration files, either standard library path, or loader-override environment variables. The official binary's hash is verified. The master additionally requires IPC, CanRegisterMaster and zero clients before initial registration. The client only starts after our master is live and has created its shared object owned by this uid. Re-registration requires CanRegisterMaster and exactly one client (our client intentionally remains alive to test reconnection).

Only normal owned master/client deregistration is performed. No MTS_Reinitialize call, configuration edits or manual shared-memory unlink occur. The runner closes input pipes on failures to request orderly exit. A stuck process or retained shared object must be investigated; the harness will not erase that state. Successful run.log ends with both normal exits, zero clients before master exit, and CLEANUP shared object absent=True.

## Explicit limitations

This validates the production adapter with the real official binary and separate-process IPC using adapted loader paths. It does NOT qualify the unchanged shipping loader/system installation, a native plug-in build, a real DAW, audio thread performance, or an external master application UI. It does NOT establish atomic/coherent snapshots during concurrent master mutation or rapid disconnect/reconnect. Updates are deliberately serialized with pipe handshakes. It does not exhaust every malformed frequency; the existing contract tests cover the wider adversarial input matrix.

Evidence: build.log, run.log, initial-run logs, source-hashes.txt, loader patches, and complete harness source/scripts. The production working tree remained clean after qualification.

## Optional direct production processor qualification

The separate `prepare_processor.py`, `build_processor.py`, `run_processor.py` and
`processor.cpp` extend this qualification through actual `Cs80AudioProcessor`
`processBlock` calls. The original adapter harness remains unchanged. This is
explicitly opt-in, excluded from default builds, and not registered with CTest
because it uses the host's shared MTS namespace.

Prepare a **new** evidence directory, build against an already configured native
JUCE build of this repository, then run both engines serially:

```sh
python tests/mts_real_library/prepare_processor.py \
  --repo /path/to/yamaha-cs80 --upstream /path/to/MTS-ESP \
  --evidence /path/to/new-processor-evidence
python /path/to/new-processor-evidence/build_processor.py \
  --evidence /path/to/new-processor-evidence \
  --native-build /path/to/existing-native-build
python /path/to/new-processor-evidence/run_processor.py --engine legacy
python /path/to/new-processor-evidence/run_processor.py --engine circuit
```

If CMake is not on PATH, pass `--cmake /path/to/cmake`. If retained verified native
libraries require an explicit runtime location, pass `--runpath /verified/lib/path`
to the build script. This sets only the optional executable's RUNPATH; do not
weaken the runner's `LD_LIBRARY_PATH`/`LD_PRELOAD`/`LD_AUDIT` refusal. No system
library installation or configuration change occurs.

The optional target links an executable-local unchanged production adapter and
the generated official upstream client before the production `CS80Reference`
static archive. Symbol inspection requires exactly one adapter `fillTable`
implementation and one `MTS_RegisterClient`. The postbuild processor manifest
records the archive, executable, fixture, production sources and resolved runtime
dependencies. The runner verifies it before either process can initialize MTS.

Coverage: complete primary and dormant-extension 16-channel tables and filters,
filtered-note allocation, real IPC updates, MPE/PURE global-table switching,
late-channel invalid bank rejection with retained tuning, disconnect, and dormant
8-to-16 activation. Sixteen fresh voices must use their retained channel pitches;
4096 output frames must be finite and have nonzero energy. Legacy and circuit
engines are separate serial runs, with normal deregistration and shared-memory
cleanup checked after each.

This exercises the production processor directly, **not** a hosted VST3, standalone
application, real DAW or shipping system loader. It is a functional audio positive
control, not spectral pitch extraction, reference-parity certification or a timing
benchmark. Master edits remain pipe-serialized; no concurrent snapshot-coherence
claim is made. Native JUCE/toolchain dependencies must already be configured by
the caller; this harness does not install them.

### Recorded processor result

On 2026-11-11, Linux x86_64 qualification passed for both legacy and circuit
engines against production source baseline `de6fd4a54098ef22dbe83f6094d8a2faca77dfa5`
and the pinned official library above. All processor stages passed, including
mode-transition table behavior and sixteen fresh correctly pitched voices after
disconnect. Finite audio output had summed squared energy 92.1583 (legacy) and
2.987 (circuit) over 4096 frames. Both processes exited with code 0 and shared
memory was absent after each run. The original adapter qualification also passed
unchanged afterward. These observations do not establish PURE audible bypass,
retuning of already-sounding notes, or hosted VST3/DAW behavior.

An independent reviewer inspected the optional linkage and fixture, then replayed
both engine runs serially on the same verified artifacts. Both independently
passed every stage with exit code 0, client count returning to zero and shared
memory absent after each run. The linked static archive is hash-pinned; the
manifest does not claim to enumerate every core `.cpp` source file.
