"""Reject missing, skipped or incomplete Node verifier runs in CI."""
import json
from pathlib import Path
import re
import sys

FIELDS = ('tests', 'suites', 'pass', 'fail', 'cancelled', 'skipped', 'todo')


def check_summary(text):
    summary = {}
    for name in FIELDS:
        values = re.findall(r'^# ' + name + r' ([0-9]+)\s*$', text, re.MULTILINE)
        if len(values) != 1:
            raise ValueError(f'Missing or duplicate Node summary field: {name}')
        summary[name] = int(values[0])
    if summary['tests'] < 8 or summary['pass'] != summary['tests']:
        raise ValueError('Missing or incomplete Node verifier inventory')
    if any(summary[name] != 0 for name in ('fail', 'cancelled', 'skipped', 'todo')):
        raise ValueError('Failed, cancelled, skipped or TODO Node verifier test')
    return summary


if __name__ == '__main__':
    if len(sys.argv) != 2:
        raise SystemExit('Usage: node_tap_guard.py TAP_LOG')
    print(json.dumps(check_summary(Path(sys.argv[1]).read_text()), sort_keys=True))
