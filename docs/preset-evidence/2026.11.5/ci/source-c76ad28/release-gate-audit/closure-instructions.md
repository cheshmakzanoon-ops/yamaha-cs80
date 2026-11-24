# A58 closure instructions for actual c76 evidence

These are verification-only corrections. Preserve the original db6b prepared
binders and checkpoint hashes. Apply changes in the new `source-c76ad28`
Windows/Mac copies and in the final Linux composition. No production, workflow,
acceptance threshold or historical parser change is required.

## F1: bind parser observations to actual archive bytes

The unchanged native parser already records `archive.path`, `archive.bytes`
and `archive.sha256` on each native and validator observation. The prepared
Windows and Mac binders consume those observations but do not compare those
archive identities with the ZIP/API receipts they independently inspect. A
stale report can therefore pass that particular join even when the acquired
ZIP is authentic. This is an omission in the preparation, not evidence that a
stale report was accepted.

After selecting the expected target and authentic artifact records, require
both size and digest explicitly before accepting any parsed result:

```python
for observed, api, label in (
    (native, read(ROOT / 'test-evidence-api.json'), 'native'),
    (validator, read(ROOT / 'pluginval-api.json'), 'validator'),
):
    require(isinstance(observed.get('archive'), dict), label + ': archive binding absent')
    require(observed['archive']['bytes'] == api['size_in_bytes'], label + ': parsed archive size differs')
    require(observed['archive']['sha256'] == api['digest'].split(':', 1)[1],
            label + ': parsed archive digest differs')
```

These assertions depend on the existing earlier check that each API size and
digest equal the actual ZIP bytes. Do not replace that actual-byte check with
metadata-to-metadata comparison.

The independent [checker](bind-parser-archives.py) additionally binds every
parsed JUnit, LastTest, JUCE, version, console, report, core and preserved-attempt
member record to the same immutable actual ZIP snapshots. This closes a stale
member-record mismatch even if the outer archive record was updated. It pins
the unchanged historical parser to SHA-256
`f44d978bcb6d0754165c8d3d55320ac63919f22b15bcb37a54506ec00db3cc58`.

Run it once for each actual platform after acquiring and parsing its real
archives. For example, with real paths substituted:

```sh
python3 docs/preset-evidence/2026.11.5/ci/source-c76ad28/release-gate-audit/bind-parser-archives.py \
  --repo /absolute/path/to/yamaha-cs80 \
  --source c76ad288e02fbeca48f39652bf4fcdd99837f793 \
  --run-id 45351731801 --attempt 1 --target windows-x86_64 \
  --observations /actual/windows/native-validator-observations.json \
  --native-zip /actual/downloads/test-evidence.zip \
  --native-api /actual/windows/test-evidence-api.json \
  --validator-zip /actual/downloads/pluginval.zip \
  --validator-api /actual/windows/pluginval-api.json \
  --out /actual/windows/parser-archive-binding.json
```

The output must be a new file. Other target names are `linux-x86_64`,
`macos-arm64` and `macos-x86_64`. A combined four-target raw parser report is
accepted, but the requested target must occur exactly once in each list.

A successful checker result is **input binding only**. It retains
`native_results_accepted: false`, `original_attempt_binding_verified: false`,
`platform_qualified: false`, and `release_qualified: false`. The coordinator
must separately require actual result semantics, original successful jobs,
original uploads, exits, retry eligibility and payload equality. Keep the raw
parser's own false release field unchanged.

## F2: bind the Windows consumer unconditionally

The A36 payload helper checks the actual Windows consumer receipt only when
its validator-input ZIP contains the optional `.exp`/`.lib` compiler sidecars.
The prepared A49 binder currently relies on that conditional path. A
sidecar-free VST3 ZIP must not remove the original consumer-ID/digest gate.

Use the timestamped, decoded original `consumer_rows`, the actual VST3 API
record, and the successful original `Download Windows VST3` API step. The
following is the same required structure already prepared for Linux:

```python
vst3 = read(ROOT / 'vst3-api.json')
download_step = step(consumer, 'Download Windows VST3')
downloaded = [(timestamp, message) for timestamp, message in consumer_rows
              if timestamp and in_step(timestamp, download_step)]
receipt = (f"- cs80-windows-vst3 (ID: {vst3['id']}, "
           f"Size: {vst3['size_in_bytes']}, Expected Digest: {vst3['digest']})")
require(sum(message == receipt for _, message in downloaded) == 1,
        'Original consumer artifact receipt differs')
require(sum(message == 'SHA256 digest of downloaded artifact is ' +
            vst3['digest'].split(':', 1)[1] for _, message in downloaded) == 1,
        'Original consumer downloaded digest differs')
require(any(message == 'Artifact download completed successfully.'
            for _, message in downloaded), 'Original consumer download incomplete')
```

When A36 supplies `validator_container_contract.consumer_log`, additionally
require its byte count and SHA-256 to equal the original raw
`validator-job.log` used by the platform binder. A36 already verifies the two
selected plugin paths on that branch. If no sidecars are present, retain the
unconditional receipt gate and verify the two original validator paths
explicitly. The sidecars remain outside the selected VST3 bundle only under
A36's existing strict format and path rules; every bundle resource remains
part of payload equality.

A direct log-identity assertion for the sidecar branch is:

```python
container = payload.get('validator_container_contract')
if container is not None:
    original = (ROOT / 'validator-job.log').read_bytes()
    bound_log = container['consumer_log']
    require(bound_log['bytes'] == len(original) and bound_log['sha256'] == sha(original),
            'A36 consumer-log bytes differ from the original platform log')
```

## Closure evidence still required

For all four targets retain the independent checker's actual output, raw parser
report, authentic API receipts and actual ZIP identities. For Windows retain
the new unconditional consumer receipt result and any A36 consumer-log identity
comparison. Then record these closures in the consolidated release gate record.
The [12-method synthetic probe](synthetic-binding-probe-results.json) validates
the small input-binding checker, not any product artifact or native result.
