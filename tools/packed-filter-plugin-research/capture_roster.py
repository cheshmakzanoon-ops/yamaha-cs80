"""Source-bound existing native fixture output inventory; never runs a fixture.

The checked-in roster enumerates exposed captures, not every in-process feature
assertion. In particular most historical noqueue/Legacy/PURE captures do not
replace active automation assertions. No-capture drivers remain mandatory.
"""
import hashlib,json,math,struct
from pathlib import Path
from identity import require,capture_files

FROZEN=Path(__file__).with_name('capture_roster_frozen.json')

def frozen():
    return json.loads(FROZEN.read_text())

def bind_contract(*, repo, raw_commands, source_root, build_root):
    contract=frozen()
    repo_path,source_path,build_path=map(lambda p:Path(p).absolute(),(repo,source_root,build_root))
    require(repo_path==source_path,'hashed repository and command source root differ')
    require(all('..' not in p.parts for p in (repo_path,source_path,build_path)),
            'noncanonical source/build path')
    for path in (repo_path,source_path,build_path):
        require(not any(p.is_symlink() for p in (path,*path.parents)), 'symlink source/build path')
    require(capture_files(repo,list(contract['source_sha256']))==contract['source_sha256'],
            'fixture source/include/configuration identity changed')
    require(isinstance(raw_commands,dict), 'complete generated command inventory required')
    observed={n:a for n,a in raw_commands.items() if isinstance(a,list) and any('.vst3/' in x for x in a)}
    require(set(observed)==set(contract['cases']), 'native fixture case inventory changed')
    source_root,build_root=map(str,(source_root,build_root))
    require(Path(source_root).is_absolute() and Path(build_root).is_absolute()
            and source_root!=build_root, 'distinct absolute source/build roots required')
    for name,case in contract['cases'].items():
        expected=[x.replace('${BUILD}',build_root).replace('${SOURCE}',source_root) for x in case['argv']]
        require(observed[name]==expected, 'original native argv changed: '+name)
    return {'native_cases':len(contract['cases']),'drivers':len(contract['drivers']),
            'exposed_capture_cases':sum(c['capture_directory'] is not None for c in contract['cases'].values()),
            'full_original_assertions_required':True,'production_qualified':False}

def capture_case(root, case_name):
    """root is an owned/sealed capture tree preserving canonical build-relative dirs.

    Roots must be quiescent and owned: hashing and finite-value reads are separate.
    Caller must bind fixture exit/argv, sealed module and original assertions to
    this case. Hash equality alone does not prove those actions occurred.
    """
    contract=frozen();require(case_name in contract['cases'],'unknown fixture case')
    case=contract['cases'][case_name];driver=contract['drivers'][case['driver']]
    if not driver['captures_exposed']:
        return {'captures_exposed':False,'full_original_assertions_required':True,
                'production_qualified':False}
    relative=case['capture_directory']
    require(relative.startswith('${BUILD}/'),'noncanonical frozen capture directory')
    root=Path(root).absolute();directory=root/relative[len('${BUILD}/'):]
    for path in (directory,*directory.parents):
        require(not path.is_symlink(),'symlink capture path')
        if path==root:break
    require(directory.is_dir(),'missing capture directory')
    require({p.name for p in directory.iterdir()}==set(driver['captures']), 'missing/extra capture file')
    hashes=capture_files(directory,list(driver['captures']))
    for name,size in driver['captures'].items():
        data=(directory/name).read_bytes()
        require(len(data)==size,'capture byte length changed')
        require(all(math.isfinite(v[0]) for v in struct.iter_unpack('<f',data)), 'nonfinite PCM capture')
    return {'captures_exposed':True,'files':len(hashes),'bytes':sum(driver['captures'].values()),
            'sha256':hashes,'full_original_assertions_required':True,'production_qualified':False}

def compare_case(stock_root,candidate_root,case_name):
    """Compare disjoint sealed trees, never a tree against itself or an alias.

    Distinct paths alone cannot establish independent stock/candidate provenance.
    The orchestrator must still bind each tree to its sealed arm and actual run.
    """
    roots=tuple(Path(p).absolute() for p in (stock_root,candidate_root))
    for root in roots:
        require(root.is_dir(),'sealed capture root missing')
        require(not any(p.is_symlink() for p in (root,*root.parents)),
                'symlink sealed capture root')
    stock,candidate=(root.resolve(strict=True) for root in roots)
    require(stock!=candidate and not stock.is_relative_to(candidate)
            and not candidate.is_relative_to(stock),'independent disjoint capture roots required')
    a=capture_case(stock,case_name);b=capture_case(candidate,case_name)
    require(a==b,'exact fixture capture mismatch')
    return {**a,'independent_sealed_arm_identity_required':True}
