"""One owned-output relink with verified recoverable copies and actual command log.

No entry point or implicit execution. Caller must first freeze the reviewed source,
configuration, target list and complete cross-arm object inventory. This helper is
not sufficient production admission. Work/build roots must be quiescent and owned.
"""
from pathlib import Path
from identity import require,capture_files


def _relink_once(*, build, sealed, expected_outputs, expected_stock_hashes, targets, commands, label):
    """Remove only known owned output files, with verified equivalent sealed copies.

    `commands` is the reviewed single-attempt Commands ledger, not a shell string.
    The output roster must include every final binary/archive/map to be proven;
    input objects may also be listed when a forced rebuild is required. This is
    deliberately independent of target-name discovery or source edits.
    """
    build,sealed=Path(build).absolute(),Path(sealed).absolute()
    require(build!=sealed and not build.is_relative_to(sealed) and not sealed.is_relative_to(build),
            'separate build and sealed roots required')
    require(isinstance(targets,list) and bool(targets) and len(set(targets))==len(targets)
            and all(isinstance(t,str) and t and all(c.isalnum() or c=='_' for c in t) for t in targets),
            'frozen literal CMake target names required')
    require(set(expected_outputs)==set(expected_stock_hashes),'complete frozen output hash roster required')
    require(isinstance(commands.ledger,list) and not commands.terminal_failure
            and not any(r.get('label')==label for r in commands.ledger), 'terminal or repeated relink forbidden')
    commands.remaining()
    before=capture_files(build,expected_outputs)
    require(before==expected_stock_hashes,'canonical prelink outputs changed')
    retained=capture_files(sealed,expected_outputs)
    require(retained==expected_stock_hashes,'sealed equivalent copies missing or changed')
    # All retained copies have been verified before any deletion. A caller must
    # preserve the terminal ledger and copies if the subsequent linker fails.
    for name in expected_outputs:
        require(capture_files(build,[name])[name]==before[name]
                and capture_files(sealed,[name])[name]==retained[name], 'output changed before removal')
        (build/name).unlink()
    require(all(not (build/name).exists() for name in expected_outputs),'prelink absence not established')
    command=['cmake','--build',str(build),'--parallel','2','--verbose','--target',*targets]
    stdout,stderr=commands.run(label,command,cwd=build)
    record=commands.ledger[-1]
    require(record.get('label')==label and record.get('argv')==command and record.get('cwd')==str(build)
            and record.get('exit')==0, 'actual successful relink ledger missing')
    log_hashes={}
    for key,path in [('stdout',Path(stdout)),('stderr',Path(stderr))]:
        log_hashes[key]=capture_files(path.parent,[path.name])[path.name]
    after=capture_files(build,expected_outputs)
    require(capture_files(sealed,expected_outputs)==retained,'sealed original outputs changed')
    return {'before':before,'equivalent_retained':retained,'absent_before_command':list(expected_outputs),
            'command':command,'ledger_record':dict(record),'stdout':str(stdout),'stderr':str(stderr),
            'log_sha256':log_hashes,'after':after,
            'production_qualified':False}


def relink_once(*, build, sealed, expected_outputs, expected_stock_hashes, targets, commands, label):
    """Latch every validation or execution failure as terminal, persist, never retry."""
    try:
        return _relink_once(build=build, sealed=sealed, expected_outputs=expected_outputs,
                            expected_stock_hashes=expected_stock_hashes, targets=targets,
                            commands=commands, label=label)
    except BaseException as error:
        commands.terminal_failure = True
        commands.ledger.append({'label':label + '-validation-failure',
                                'error':repr(error), 'terminal_failure':True})
        commands.persist()
        raise
