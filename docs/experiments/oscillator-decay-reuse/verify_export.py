"""Integrity and offline statistics replay only; never launches native code."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parent
manifest = json.loads((root / 'export-manifest.json').read_text())
assert manifest['schema'] == 'oscillator-decay-reuse-portable-export-v1'
for name, row in manifest['files'].items():
    p = root / name
    assert p.resolve().is_relative_to(root.resolve()), name
    data = p.read_bytes()
    assert len(data) == row['export_bytes'], name
    assert hashlib.sha256(data).hexdigest() == row['export_sha256'], name
    assert b'/workspace/' not in data and b'/home/agent/' not in data, name
    if not row['path_redacted']:
        assert row['original_sha256'] == row['export_sha256'], name
with tempfile.TemporaryDirectory() as td:
    dest = Path(td)
    for name in ('analyze.py', 'aa.csv', 'ab.csv'):
        shutil.copyfile(root / 'cost' / name, dest / name)
    subprocess.run([sys.executable, str(dest / 'analyze.py')], check=True,
                   stdout=subprocess.DEVNULL, timeout=30)
    actual = json.loads((dest / 'summary.json').read_text())
    assert actual == json.loads((root / 'cost/summary.json').read_text())
    assert actual['decision'] == 'NO_ADVANCE_GAIN_OR_UNCERTAIN_GUARD'
print(f"PASS: {len(manifest['files'])} file hashes; all 384 rows and frozen statistical summary replayed; no native execution")
