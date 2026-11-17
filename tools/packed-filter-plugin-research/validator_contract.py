"""Frozen Linux validator breadth with strict single-attempt receipt admission.

Pure command construction/checking, no executable entry point or implicit launch.
The normal release retry wrapper is deliberately not used: research rejects the
first failure. Downloaded validator byte/version provenance remains a separate gate.
The orchestrator must construct expected_argv from linux_validations, bind log
bytes and actual ledger records, require exactly one attempt per case and latch
all failures terminal. Headless/GUI deadlines and log completeness are not proved
by this pure helper.
"""
from pathlib import Path
from identity import require


def linux_validations(*, repo, validator, bundle, output):
    repo, validator, bundle, output = map(Path, (repo, validator, bundle, output))
    require(all(p.is_absolute() and '..' not in p.parts for p in (repo,validator,bundle,output)),
            'absolute canonical validation paths required')
    result = {}
    for name in ('headless','gui','gui-seed-regression'):
        prefix=[] if name=='headless' else ['xvfb-run','-a','-s','-screen 0 1920x1200x24']
        if name=='gui-seed-regression':
            prefix=['timeout','--signal=TERM','--kill-after=10s','240s',*prefix]
        argv=[*prefix,'python3',str(repo/'scripts/run-pluginval-with-core.py'),
              '--core-log',str(output/(name+'-core.log')),'--executable',str(validator),'--',str(validator)]
        if name=='gui-seed-regression':argv+=['--random-seed','0x2da72db']
        argv+=['--strictness-level','10','--validate-in-process']
        if name=='headless':argv+=['--skip-gui-tests']
        argv+=['--timeout-ms','120000','--output-dir',str(output/'reports'),
               '--output-filename','cs80-'+name,str(bundle)]
        result[name]=argv
    return result


def admit_validation(*, name, expected_argv, record, stdout, stderr, arm=None):
    require(name in ('headless','gui','gui-seed-regression'), 'unknown validation case')
    require(arm in (None,'stock','candidate'), 'unknown validation arm')
    label=name if arm is None else arm+'-'+name
    require(record.get('label')==label and record.get('argv')==expected_argv,
            'validator command/identity drift')
    require(record.get('exit')==0 and not record.get('error'), 'validator failed including unload')
    require(isinstance(stdout,str) and isinstance(stderr,str), 'complete validator logs required')
    text=stdout+'\n'+stderr
    require('SUCCESS' in text.splitlines() and 'FAILED' not in text,
            'validator requires clean explicit SUCCESS')
    return {'case':name,'single_attempt_required':True,'production_qualified':False}
