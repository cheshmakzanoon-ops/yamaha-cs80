"""Seal/clear only source-bound native fixture captures, never execute fixtures.

Writers must be stopped. A clear removes only exact generated capture files after
an independently located equivalent sealed copy is byte-verified. This is not a
fixture result: original argv, process status, assertions and module identity are
mandatory adjoining evidence. Failure must latch the orchestrator terminal.
"""
from pathlib import Path
import shutil
from identity import require,capture_files
from capture_roster import frozen,capture_case


def roots(build,sealed):
    build,sealed=Path(build).absolute(),Path(sealed).absolute()
    for root in (build,sealed):
        require('..' not in root.parts,'canonical capture roots required')
        require(not any(p.is_symlink() for p in (root,*root.parents)),'symlink capture root forbidden')
        require(root.is_dir(),'existing owned capture root required')
    require(build!=sealed and not build.is_relative_to(sealed) and not sealed.is_relative_to(build),'independent capture roots required')
    return build,sealed


def directory_for(root,case):
    record=frozen()['cases'].get(case)
    require(record is not None and record['capture_directory'] is not None,'exposed capture case required')
    relative=record['capture_directory']
    require(relative.startswith('${BUILD}/'),'frozen canonical capture directory required')
    return root/relative[len('${BUILD}/'):]


def seal_case(*,build,sealed,case):
    build,sealed=roots(build,sealed)
    before=capture_case(build,case);source=directory_for(build,case);target=directory_for(sealed,case)
    require(not target.exists() and not target.is_symlink(),'capture seal already exists')
    require(target.parent==sealed,'direct owned capture directory required')
    shutil.copytree(source,target,symlinks=True,copy_function=shutil.copy2)
    require(capture_case(build,case)==before and capture_case(sealed,case)==before,'capture changed during sealing')
    return before


def clear_case(*,build,sealed,case,expected):
    build,sealed=roots(build,sealed)
    require(capture_case(build,case)==capture_case(sealed,case)==expected,'verified equivalent capture seal required')
    source=directory_for(build,case);copy=directory_for(sealed,case)
    for name,wanted in expected['sha256'].items():
        require(capture_files(source,[name])[name]==wanted and capture_files(copy,[name])[name]==wanted,
                'capture changed before recoverable removal')
        (source/name).unlink()
    source.rmdir()
    require(not source.exists() and capture_case(sealed,case)==expected,'capture removal/seal verification failed')
    return {'case':case,'canonical_capture_absent':True,'equivalent_seal_preserved':True,'production_qualified':False}
