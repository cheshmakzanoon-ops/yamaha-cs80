#!/usr/bin/env python3
"""Independent bounded review of A58's exact-file Git checkout correction."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[5]
HERE = Path(__file__).resolve().parent
SOURCE = 'fcc3c8e055223fe4f786ca162ff46076c33471c8'
BASELINE = 'docs/preset-evidence/2026.11.5/baseline/inventory.json'
FILES = [BASELINE, 'scripts/compare-preset-response-2026-11-6.py',
         'scripts/test-compare-preset-response-2026-11-6.py']


def run(args, cwd, data=None):
    return subprocess.run(args, cwd=cwd, input=data, capture_output=True, check=True).stdout


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    original = {name: run(['git','show', SOURCE + ':' + name], ROOT) for name in FILES}
    for name, raw in original.items():
        assert (ROOT/name).read_bytes() == raw, name + ' has changed'
    attributes = (ROOT/'.gitattributes').read_bytes()
    rules = [x.strip() for x in attributes.decode().splitlines() if x.strip() and not x.startswith('#')]
    assert rules == [BASELINE + ' -text'], rules
    with tempfile.TemporaryDirectory(prefix='cs80-a60-review-') as directory:
        root = Path(directory)
        run(['git','init','--quiet'], root)
        run(['git','config','core.autocrlf','true'], root)
        run(['git','config','core.eol','crlf'], root)
        for name, raw in {**original, '.gitattributes': attributes}.items():
            oid = run(['git','hash-object','-w','--stdin'], root, raw).decode().strip()
            run(['git','update-index','--add','--cacheinfo', '100644,' + oid + ',' + name], root)
        run(['git','checkout-index','--all','--force'], root)
        actual = (root/BASELINE).read_bytes()
        assert actual == original[BASELINE]
        text_attribute = run(['git','check-attr','text','--',BASELINE],root).decode().strip()
        assert text_attribute == BASELINE + ': text: unset'
        untouched = run(['git','check-attr','text','--','scripts/compare-preset-response-2026-11-6.py'],root).decode().strip()
        assert untouched.endswith(': text: unspecified'), untouched
        result = subprocess.run([sys.executable, str(root/FILES[2])], cwd=root, capture_output=True)
        log = result.stdout + result.stderr
        assert result.returncode == 0 and b'Ran 7 tests' in log and b'\nOK\n' in log, log.decode()
        (HERE/'independent-checkout.log').write_bytes(log)
    a58_path = ROOT/'docs/preset-evidence/2026.11.6/verification/a58/checkout-regression.json'
    a58 = json.loads(a58_path.read_bytes())
    assert a58['attributes_sha256'] == sha(attributes)
    assert len(a58['fixed_checkout_cases']) == 6
    assert all(x['test_returncode'] == 0 and x['contract_test_groups'] == 7
               and x['checked_out_baseline']['sha256'] == sha(original[BASELINE])
               for x in a58['fixed_checkout_cases'])
    assert a58['original_failure_reproduced']['test_returncode'] == 1
    assert len(a58['negative_authentication']['controls']) == 4
    assert all(x['returncode'] == 1 and x['rejected_by_raw_byte_pin']
               and x['input_and_report_unchanged'] for x in a58['negative_authentication']['controls'])
    report = {
        'schema':1,'status':'scoped_checkout_fix_review_passed', 'native_windows_after_fix_qualified':False,
        'release_qualified':False, 'original_failed_source':SOURCE,
        'attributes':{'sha256':sha(attributes),'bytes':len(attributes),'sole_effective_rule':rules[0]},
        'frozen_inputs_unchanged':{name:{'sha256':sha(raw),'bytes':len(raw)} for name,raw in original.items()},
        'independent_exercise':{'core_autocrlf':'true','core_eol':'crlf','actual_checkout':True,
            'baseline_bytes_identical':True,'baseline_text_attribute':text_attribute,
            'comparator_text_attribute':untouched,'returncode':0,'existing_contract_test_groups':7,
            'log_sha256':sha(log)},
        'reviewed_a58_result':{'path':str(a58_path.relative_to(ROOT)), 'sha256':sha(a58_path.read_bytes()),
            'fixed_checkout_modes':6,'changed_byte_controls_rejected':4},
        'limitations':['This local Git checkout exercise is not execution on Windows.',
            'The original native failure remains failed; fresh native and validator runs are required.',
            'No audio quality, plugin runtime, or public artifact acceptance follows from this correction.']}
    (HERE/'fix-review.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS: exact-file attribute preserves pinned bytes; unchanged seven-group contract passes independent autocrlf checkout.')


if __name__ == '__main__':
    main()
