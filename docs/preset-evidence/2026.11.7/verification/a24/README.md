# A24 reproduction

This evidence uses frozen current source
`c5549f7d095a5c5f49a5ad22ec0ccc819245d201` and the separately built baseline
`18711f93a39aa49f3d6f9a41784be456224facbe`. The supplied core archives are
`/tmp/cs80-build-1cee5a/libcs80_core.a` and
`/tmp/cs80-reference-1cee5a/libcs80_core.a`. Their digests, source checks,
compiler, executable digests and commands are in `review.json`.

The baseline probe **must use the complete archived old source/header tree**.
Using current `InstrumentBody.h` with the old archive changes `Engine` layout
and invalidates the caller ABI. Do not substitute only the old body header.

## Captures and checker

Run from the frozen repository root. The following creates a new, isolated
temporary directory and refuses to reuse one. It verifies the known core
archives and current source before building, then renders exactly the declared
18 current and two baseline cases. It does not mutate the shared build.

```sh
python3 - <<'PY'
from pathlib import Path
import hashlib, io, json, subprocess, tarfile

repo = Path.cwd()
evidence = repo / 'docs/preset-evidence/2026.11.7/verification/a24'
record = json.loads((evidence / 'review.json').read_text())
task_dir = Path('/tmp/cs80-a24-1cee5a-replay')
for row in record['core_archives']:
    assert hashlib.sha256(Path(row['path']).read_bytes()).hexdigest() == row['sha256']
for row in record['source_receipts']:
    assert hashlib.sha256((repo / row['path']).read_bytes()).hexdigest() == row['sha256']
task_dir.mkdir(exist_ok=False)
for child in ['current', 'baseline', 'oldsrc']:
    (task_dir / child).mkdir()
source = record['source_baseline']
archive = subprocess.check_output(['git', 'archive', source, 'src'])
with tarfile.open(fileobj=io.BytesIO(archive)) as files:
    files.extractall(task_dir / 'oldsrc', filter='data')
commands = {}
for label, include, core in [
    ('current', repo / 'src', Path(record['core_archives'][0]['path'])),
    ('baseline', task_dir / 'oldsrc/src', Path(record['core_archives'][1]['path']))]:
    command = ['c++', '-std=c++20', '-O2', '-I' + str(include),
               str(evidence / 'sequence-probe.cpp'), str(core),
               '-o', str(task_dir / ('probe-' + label))]
    commands[label] = command
    subprocess.run(command, check=True)
    with (task_dir / (label + '.jsonl')).open('w') as stream:
        subprocess.run([str(task_dir / ('probe-' + label)), label,
                        str(task_dir / label)], stdout=stream, check=True)
setup = {
    'source_current': record['source_current'], 'source_baseline': source,
    'archive_extraction': 'Complete archived src tree for baseline ABI compatibility',
    'baseline_source_archive_sha256': hashlib.sha256(archive).hexdigest(),
    'commands': commands,
    'compiler': subprocess.check_output(['c++', '--version'], text=True).splitlines()[0]}
(task_dir / 'build-setup.json').write_text(json.dumps(setup, indent=2) + '\n')
subprocess.run(['python3', str(evidence / 'review.py'), '--repo', str(repo),
                '--tmp', str(task_dir), '--out', str(task_dir / 'review.json')], check=True)
PY
```

The checker requires NumPy. It reads only filenames explicitly emitted by its
own probe, checks their sizes, records SHA-256 values, compares complete PCM
bytes for the tempo controls, and calculates the timing, motion and articulation
metrics. Raw float32 stereo PCM stays in the declared temporary directory;
small telemetry, reports and checks are retained with the review.

`review.py` also verifies the historical `.6` complete-family review and the
current focused CTest log/XML digests. It reads the recorded current lifecycle
receipts from this evidence directory; it does not silently rerun the broad
core or modern refinement suites. The three cheap existing lifecycle binaries
can be rerun independently:

```sh
/tmp/cs80-build-1cee5a/cs80_arp_rate_lifecycle_tests
/tmp/cs80-build-1cee5a/cs80_arp_pedal_lifecycle_tests
/tmp/cs80-build-1cee5a/cs80_arp_repeat_lifecycle_tests
```

## Coverage boundaries

The current probe covers eight unique complete presets. The normal eight
48 kHz phrases, four transport controls, three zero-LFO-depth controls and one
zero-ring-amount control account for 16 renders. Two bell presets also run at
44.1 kHz in each revision, yielding 20 renders overall. The old A28 complete
27-program phrase result remains historical evidence. Single-note capture CLI
statistics are not treated as proof of multi-key arpeggiator order or timing.
