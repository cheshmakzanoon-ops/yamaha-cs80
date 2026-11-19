# Phase Q only: one predeclared stock/stock stage

This is **not** a general plug-in timer. There is no candidate argument or C-stage path. Both independent peer processes must load the same inode and SHA-256 of the approved cutoff-stock native ELF. [The protocol](PROTOCOL.md) fixes all five scenes, both quanta, all 48 serial slots, budgets, spacing, controls and numerical gates. A failed or partial run does not admit anything.

Build the independent Q executable against the pinned VST3 SDK. The build applies a reviewed seven-site adapter to the SHA-identified no-clock worker; the shipped plug-in is not changed. If the zero-worker source differs, the build aborts. In the absence of a supervisor-issued Q seal environment this binary is strictly a zero-callback-clock worker. Before sealing, exercise the exact Q executable with a full isolated/peer dry run. It must match the [archived reference](zero-preflight/README.md) at every scene and continuation. Do not benchmark alongside native compilation, another sampler/timer or a second stage.

```sh
python3 -B build_q_host.py /path/to/JUCE/VST3_SDK /tmp/new-q-host
python3 -B preflight.py --scope full --host /tmp/new-q-host --stock /path/to/known-cutoff.so --out /tmp/new-q-host-dry
python3 -B phase_q.py seal --host /tmp/new-q-host --stock /path/to/known-cutoff.so \
  --full-dry /tmp/new-q-host-dry --out /tmp/new-fixed-seal
python3 -B -m unittest -v test_preflight test_crossover
```

**Sealing does not start native timing.** Only when this independently chosen seal and the machine window have been approved, the launch is `python3 -B phase_q.py run --sealed /tmp/new-fixed-seal --accept-seal <exact-seal-sha256>`. The first attempted launch consumes the directory's one-shot latch. There is no resume, C treatment, success-based early stopping, exclusion of slow callbacks, or automatic candidate action.

Each actual measured callback is enclosed by four ordered integer clock reads; 128 separately retained empty inner/outer brackets per lane **follow** each complete scene window. Resources and IPC are outside the brackets. The logical baton is encoded as one checked 32-bit sequence word in each direction over a Unix sequence-packet socket, so a stale acknowledgment cannot accidentally release a different visit. Workers share a physical stock file but never a process, and the supervisor stays on a separate allowed logical CPU. It rechecks the exact source, loader file, observed CPU pin and schedule; compares full PCM/FP/MIDI/continuation/public state to the isolated reference outside measurement; then discards **only** newly generated duplicates proved byte-equal to that retained reference. Original real and empty clocks, OS fields, order ledgers, diagnostics, identities, hashes and failures remain. Empty overhead is not subtracted. The observations are thread CPU in a guest, not DAW utilization or hardware performance.

The final stage file is issued only after all 48 slots verify and the collector independently reconstructs the normalized cells from the original clock rows. One shared wall origin is subtracted; thread CPU keeps each worker's native process-lifetime origin. The original CSVs are preserved. The complete normalize/reduce uses 240 scene cells, not 276,480 independent statistical replicates. `terminal.json` distinguishes failure to acquire from failing numerical gates. No C launch is authorized by either its return code or a successful numeric replay; a separate lock, native parity proof and live sentinels would be needed.
