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
