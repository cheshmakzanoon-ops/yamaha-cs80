# Zero-clock native admission, not a speed measurement

The [original machine receipt](receipt.json) records the fixed cutoff-stock ELF,
host build IDs/source hashes, complete force/readback workload, resource bounds
and single-cell resource estimate. The one isolated process and both independent
pinned peer processes matched across **40,404 audio callbacks and 60 public state
snapshots per worker**. This includes eight **different** velocities for varied
scenes, and every note/read/FP/PCM byte through a separate audible continuation.
Each worker completed 1,840 socket-scheduled visits (one isolated control recorded
the same visits without IPC). There were no inner or empty callback clocks and no
candidate process. This does not establish CPU precision or speed.

The full isolated reference retained with the original run includes PCM,
FP/MIDI traces, guards, serialized public state, declared controls and planned
schedule, plus both peers’ ordered visit ledgers. The repository publishes
the receipt rather than a duplicate binary recording of those histories.
Fresh preflight builds can reproduce the comparisons using the procedure
below. The receipt pins the analyzer code present at acquisition; a subsequent
change rejects reordered offline records without changing the schedule.

Build/read the [procedure](../PROTOCOL.md) first; from its directory:

```sh
sh build-host.sh /path/to/juce/VST3_SDK /tmp/new-pair-zero-host
python3 -B preflight.py --host /tmp/new-pair-zero-host --stock /path/to/verified-cutoff.so \
  --scope wake-two --out /tmp/new-two-wake
python3 -B preflight.py --host /tmp/new-pair-zero-host --stock /path/to/verified-cutoff.so \
  --scope full --out /tmp/new-full-dry
```

The frozen two-worker slot took 26.54 seconds elapsed and 26.38 pooled worker CPU
seconds. Multiplying a single dry slot and applying mandatory epoch spacing
projects ~23.8 minutes elapsed and ~21.1 worker CPU minutes for **Q**, before
real clocks, external validation and drift. These whole-process numbers are for
campaign sizing only; they are not callback performance numbers or instructions
to skip qualification.
