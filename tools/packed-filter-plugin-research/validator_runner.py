"""Explicit single-arm validator execution; import is inert, no CLI/workflow.

Requires caller-owned quiescent roots and independently sealed original module,
validator/version and complete bundle resources. Same Commands ledger must span
both arms and all other stages. This does not perform those preceding gates.
"""
from pathlib import Path
from identity import require,capture_files
from validator_contract import linux_validations,admit_validation


def run_validations(*, arm, repo, validator, bundle, output, commands,
                    expected_validator_sha256, module_root, module_hashes, case_timeouts):
    try:
        require(arm in ('stock','candidate'), 'explicit arm required')
        require(isinstance(case_timeouts,dict) and set(case_timeouts)=={'headless','gui','gui-seed-regression'}
                and all(isinstance(v,(int,float)) and 0<v<=3600 for v in case_timeouts.values()),
                'reviewed finite per-case deadlines required')
        cases=linux_validations(repo=repo,validator=validator,bundle=bundle,output=output)
        labels={arm+'-'+name for name in cases}
        require(not commands.terminal_failure and not any(r.get('label') in labels for r in commands.ledger),
                'terminal or repeated validation forbidden')
        require(Path(bundle).absolute()==Path(module_root).absolute(), 'validated bundle and sealed root differ')
        def bundle_identity():
            root=Path(module_root)
            paths=list(root.rglob('*'))
            require(not any(p.is_symlink() for p in paths), 'bundle symlink forbidden')
            require({str(p.relative_to(root)) for p in paths if p.is_file()}==set(module_hashes),
                    'complete bundle file inventory required')
            return capture_files(root,list(module_hashes))
        validator=Path(validator)
        require(capture_files(validator.parent,[validator.name])[validator.name]==expected_validator_sha256,
                'validator bytes differ')
        results={}
        for name,argv in cases.items():
            require(bundle_identity()==module_hashes,'module/bundle changed before validator')
            # Outer whole-process timeout complements pluginval's per-test timeout.
            # The outer job/Commands global deadline must also remain active.
            stdout,stderr=commands.run(arm+'-'+name,argv,cwd=repo,timeout=case_timeouts[name])
            record=commands.ledger[-1]
            require(record.get('cwd')==str(repo), 'validator cwd drift')
            require(bundle_identity()==module_hashes,'module/bundle changed during validator')
            require(capture_files(validator.parent,[validator.name])[validator.name]==expected_validator_sha256,
                    'validator bytes changed during execution')
            paths={'stdout':Path(stdout),'stderr':Path(stderr)}
            hashes={k:capture_files(p.parent,[p.name])[p.name] for k,p in paths.items()}
            receipt=admit_validation(name=name,expected_argv=argv,record=record,
                                     stdout=paths['stdout'].read_text(),stderr=paths['stderr'].read_text(),arm=arm)
            results[name]={**receipt,'ledger_record':dict(record),'log_sha256':hashes}
        return {'arm':arm,'validators':results,'production_qualified':False}
    except BaseException as error:
        commands.terminal_failure=True
        commands.ledger.append({'label':str(arm)+'-validator-admission-failure','error':repr(error),'terminal_failure':True})
        commands.persist()
        raise
