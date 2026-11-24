"""A single source-velocity mutation must be rejected by the phrase checker."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
BASE = Path(__file__).resolve().parent
rows = [json.loads(s) for s in (BASE / 'sequence-phrase-raw.jsonl').read_text().splitlines()]
row = next(r for r in rows if r['type'] == 'event' and r['note'] >= 0)
original = row['velocity']
row['velocity'] = .99
with tempfile.TemporaryDirectory() as temp:
    raw = Path(temp) / 'bad-velocity.jsonl'
    out = Path(temp) / 'review.json'
    raw.write_text(''.join(json.dumps(r) + '\n' for r in rows))
    result = subprocess.run([sys.executable, str(BASE / 'review-sequences.py'), '--repo', str(BASE.parents[4]), '--raw', str(raw), '--out', str(out)], capture_output=True, text=True)
    report = json.loads(out.read_text())
    failures = report['failures']
    assert result.returncode == 1 and len(failures) == 1
    assert 'source channel/velocity' in failures[0]['label']
    print(json.dumps({'exit_code':result.returncode,'mutated_program':row['name'],'old_velocity':original,'new_velocity':row['velocity'],'checks':report['counts']['checks'],'expected_failures':failures}, indent=2))
