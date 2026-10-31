import difflib
import hashlib
import json
import pathlib
import shutil
import subprocess
import sys
import time

root = pathlib.Path('/workspace/scratch/2f805bffd21a/cs80-automation-fix')
out = pathlib.Path('/tmp/cs80-au-publication-qualification')
out.mkdir(exist_ok=True)
assert len(sys.argv) == 2 and len(sys.argv[1]) == 40, 'pass the published ownership preregistration full SHA'
headers = ['AuAutomationHooks.inc', 'AuAutomationQueue.h', 'HostAutomationBridge.h']
source = {name: (root / 'src/plugin' / name).read_text() for name in headers}
mutations = [
    ('discard-normalized-publication', 'AuAutomationHooks.inc',
     'return cs80AuPublications[static_cast<size_t> (lane)].read (current);', 'return current;',
     'preserves its accepted normalized Float32'),
    ('ignore-changed-getter', 'AuAutomationQueue.h',
     'return readback == current ?', 'return true ?',
     ['changed underlying storage', 'concurrent AU readers never combine']),
    ('ignore-external-invalidation', 'AuAutomationQueue.h',
     'void invalidate() noexcept { revision.fetch_add(1); }', 'void invalidate() noexcept {}',
     'external notification invalidates'),
    ('remove-protected-readback', 'AuAutomationHooks.inc',
     'lane >= 0 && client != nullptr && client->usesHostAutomationTiming()',
     'lane >= 0 && client != nullptr', 'Pure/Legacy retain the original storage getter'),
    ('remove-lifecycle-reset', 'AuAutomationHooks.inc',
     'for (auto& publication : cs80AuPublications) publication.invalidate();',
     '// mutation: retain old publication ownership', 'lifecycle reset discards'),
    ('claim-newer-invalidation-token', 'AuAutomationQueue.h',
     'committedRevision.store(token);', 'committedRevision.store(revision.load());',
     'external invalidation during commit cannot be overwritten'),
]
report = {
    'registered_commit': 'aa3cadb4147ca9a1cd1cfe843345fcefa3d97432',
    'ownership_registration': sys.argv[1],
    'test_sha256': hashlib.sha256((root / 'tests/AuAutomationBridgeContractTests.cpp').read_bytes()).hexdigest(),
    'fixed_header_sha256': {name: hashlib.sha256(value.encode()).hexdigest() for name, value in source.items()},
    'runs': [],
}

def run(name, override=None, intended=None):
    case = out / name
    (case / 'include/plugin').mkdir(parents=True, exist_ok=True)
    for header, original in source.items():
        contents = override[1] if override is not None and header == override[0] else original
        (case / 'include/plugin' / header).write_text(contents)
    command = ['c++', '-std=c++20', '-O3', '-DNDEBUG', '-pthread', '-I', str(case/'include'),
               '-I', str(root/'src'), '-I', str(root/'tests'),
               str(root/'tests/AuAutomationBridgeContractTests.cpp'), '-o', str(case/'bridge')]
    with (case / 'build.log').open('w') as log:
        subprocess.run(command, check=True, stdout=log, stderr=subprocess.STDOUT)
    execute = [str(case/'bridge'), str(root/'tests/parity/tolerances-v2/host-automation.json')]
    start = time.monotonic()
    with (case / 'test.log').open('w') as log:
        result = subprocess.run(execute, stdout=log, stderr=subprocess.STDOUT)
    failures = (case/'test.log').read_text().splitlines()
    entry = {'name':name, 'compile_command':command, 'test_command':execute,
             'exit_code':result.returncode,'failures':failures,'seconds':time.monotonic()-start,
             'test_log':str(case/'test.log'),'executable_sha256':hashlib.sha256((case/'bridge').read_bytes()).hexdigest()}
    if intended:
        expected = intended if isinstance(intended, list) else [intended]
        entry['expected_assertions'] = expected
        assert result.returncode == 1 and all(any(item in line for line in failures) for item in expected), entry
    else:
        assert result.returncode == 0, entry
    report['runs'].append(entry)
    (out/'bridge-mutations.json').write_text(json.dumps(report, indent=2)+'\n')
    print(json.dumps({'name':name,'exit_code':result.returncode,'failures':failures}), flush=True)

fixed_source = source
source = {name: subprocess.run(['git','show','HEAD:src/plugin/'+name],cwd=root,
          capture_output=True,text=True,check=True).stdout for name in headers}
report['original_header_sha256'] = {name:hashlib.sha256(text.encode()).hexdigest() for name,text in source.items()}
run('unmodified-control', intended='preserves its accepted normalized Float32')
source = fixed_source
run('fixed-control')
for name, header, before, after, intended in mutations:
    assert source[header].count(before) == 1, name
    changed = source[header].replace(before, after)
    (out/(name+'.patch')).write_text(''.join(difflib.unified_diff(
        source[header].splitlines(True), changed.splitlines(True),
        fromfile='a/src/plugin/'+header, tofile='b/src/plugin/'+header)))
    run(name, (header, changed), intended)
run('restored-control')
report['result'] = 'all six intended mutations detected; fixed and restored controls pass'
(out/'bridge-mutations.json').write_text(json.dumps(report, indent=2)+'\n')
