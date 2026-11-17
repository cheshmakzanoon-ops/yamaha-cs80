"""Packed coefficient research only. One immutable attempt; no production or plugin lifecycle work."""
import argparse, hashlib, json, os, platform, shutil, signal, subprocess, sys, time
from pathlib import Path
import analysis
from protocol_math import schedule
from pcm_store import canonicalize

SOURCE='702bf5ee4e375c7b13da7ec95b01b56587bbf75c'
TESTS='64b7f72ed0139d846e8107c47c62f190cb13eefa'
FLAGS=['-std=gnu++20','-O3','-DNDEBUG','-fPIC']
def compile_flags(unit=None):
    return FLAGS+(['-ffp-contract=off'] if unit=='Ig00156Batch' else [])
def require(ok,msg):
    if not ok: raise RuntimeError(msg)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open('rb') as f:
        for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
    return h.hexdigest()
def write(p,value):p.write_text(json.dumps(value,indent=2)+'\n')
def inventory(root):
    result={}
    for p in sorted(root.rglob('*')):
        require(not p.is_symlink(),'symlink in owned evidence')
        if p.is_file():result[str(p.relative_to(root))]=sha(p)
    return result

# This wrapper restores the parent's original signal mask before exec. The parent
# blocks SIGALRM only until Popen has returned an assigned, owned process handle.
CHILD_EXEC = "import os,signal,sys; signal.pthread_sigmask(signal.SIG_SETMASK,{int(x) for x in sys.argv[1].split(',') if x}); os.execvp(sys.argv[2],sys.argv[2:])"
def execute_owned(argv, *, cwd, stdin, stdout, stderr, timeout, record):
    proc=None
    oldmask=signal.pthread_sigmask(signal.SIG_BLOCK,{signal.SIGALRM})
    def cleanup():
        if proc is not None:
            try:os.killpg(proc.pid,signal.SIGKILL)
            except ProcessLookupError:pass
            finally:proc.wait()
            record['exit']=proc.returncode
    try:
        wrapped=[sys.executable,'-c',CHILD_EXEC,','.join(str(int(x)) for x in sorted(oldmask)),*map(str,argv)]
        record['launch_argv']=wrapped
        proc=subprocess.Popen(wrapped,cwd=cwd,stdin=subprocess.PIPE if stdin is not None else subprocess.DEVNULL,stdout=stdout,stderr=stderr,start_new_session=True)
        record['pid']=proc.pid
        # Delivery of an alarm queued during spawn is now inside cleanup's try.
        signal.pthread_sigmask(signal.SIG_SETMASK,oldmask)
        proc.communicate(input=stdin,timeout=timeout)
        record['exit']=proc.returncode
        if proc.returncode!=0:cleanup() # Failed leaders may leave live descendants.
    except BaseException:
        cleanup();record['interrupted']=True;raise
    finally:
        signal.pthread_sigmask(signal.SIG_SETMASK,oldmask)

def validate_shared_links(links):
    for name in ['driver','batch-tests','engine-tests']:
        arms=[links[arm+'-'+name]['objects'] for arm in ['stock','candidate']]
        common=[{p:h for p,h in a.items() if Path(p).name!='Ig00156Batch.o'} for a in arms]
        require(common[0]==common[1],'shared link object paths/bytes differ')
        require(set(Path(p).name for p in common[0])==({name+'.o'} if name=='batch-tests' else {'Engine.o','FactoryPresets.o',name+'.o'}),'unexpected shared objects')
        ig=[{p:h for p,h in a.items() if Path(p).name=='Ig00156Batch.o'} for a in arms]
        require(all(len(a)==1 for a in ig) and list(ig[0].values())!=list(ig[1].values()),'missing/distinct Ig objects required')

CANDIDATE='a612510e598be7471aa8ced68b57e56868cd7345335dc475d44bfab3758bb6fd'
def main():
    ap=argparse.ArgumentParser();ap.add_argument('--repo',required=True);ap.add_argument('--revision',required=True);ap.add_argument('--out',required=True);a=ap.parse_args()
    here=Path(__file__).resolve().parent;repo=Path(a.repo).resolve();out=Path(a.out).absolute()
    require(not out.exists(),'attempt output already exists; no retry');out.mkdir();(out/'commands').mkdir()
    start=time.monotonic();deadline=start+10800;ledger=[];bindings={};status={'decision':'STOP_NO_RETRY','reason':'interrupted before completion'}
    def expired(*_):raise TimeoutError('180-minute global hard deadline')
    signal.signal(signal.SIGALRM,expired);signal.alarm(10800)
    def run(label,argv,*,cwd=None,stdin=None,limit=600):
        remaining=deadline-time.monotonic();require(remaining>0,'global deadline')
        record={'index':len(ledger),'label':label,'argv':list(map(str,argv)),'cwd':str(cwd or out),'start_elapsed':time.monotonic()-start,'limit_seconds':limit};ledger.append(record)
        log=out/'commands'/f'{record["index"]:03d}-{label}';write(out/'ledger.json',ledger)
        with log.with_suffix('.stdout').open('wb') as stdout,log.with_suffix('.stderr').open('wb') as stderr:
            try:execute_owned(record['argv'],cwd=cwd or out,stdin=stdin,stdout=stdout,stderr=stderr,timeout=min(limit,remaining),record=record)
            finally:record['end_elapsed']=time.monotonic()-start;write(out/'ledger.json',ledger)
        require(record['exit']==0,f'failed command {label}')
        return log.with_suffix('.stdout').read_bytes()
    def verify_bindings():require(all(sha(Path(p))==h for p,h in bindings.items()),'frozen input/source/object/binary changed')
    try:
        require(os.environ.get('GITHUB_RUN_ATTEMPT')=='1' and os.environ.get('GITHUB_RUN_NUMBER')=='1','first attempt only')
        require(os.environ.get('RUNNER_ENVIRONMENT')=='github-hosted' and os.environ.get('RUNNER_ARCH')=='X64','standard hosted x64 only')
        event=json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text());require(event['repository']['private'] is False,'public free-runner scope only')
        admission=json.loads((here/'admission.json').read_text())
        require(admission['review_status']=='APPROVED' and admission['independent_review_pass'] is True,'unapproved frozen inputs')
        require(admission['source_tree']==SOURCE and admission['test_tree']==TESTS and admission['candidate_sha256']==CANDIDATE,'source identity mismatch')
        require(admission['sanitizer_address_undefined_pass'] is True,'local sanitizer qualification absent')
        required={'orchestrate.py','driver.cpp','analysis.py','protocol_math.py','pcm_store.py','PROTOCOL.md','candidate.patch','component.cpp','workflow.yml','sanitizer-evidence/RESULT.json'}
        required.update(f'parameter-oracle/row-{row}.csv' for row in range(7))
        require(required.issubset(admission['files']),'missing admission input binding')
        for rel,digest in admission['files'].items():
            p=here/rel;require(not Path(rel).is_absolute() and '..' not in Path(rel).parts and p.is_file() and not p.is_symlink(),'unsafe admission path');require(sha(p)==digest,'admission hash mismatch: '+rel);bindings[str(p)]=digest
        san=json.loads((here/'sanitizer-evidence/RESULT.json').read_text())
        require(san['outcome']=='ASAN_UBSAN_EXPLICIT_OR_COMPONENT_PASS' and san['variant_sha256']==CANDIDATE,'wrong local sanitizer candidate')
        bindings[str(here/'admission.json')]=sha(here/'admission.json')
        workflow=repo/'.github/workflows/packed-coefficient-cpu-study.yml'
        require(sha(workflow)==sha(here/'workflow.yml'),'installed workflow differs from frozen copy');bindings[str(workflow)]=sha(workflow)
        shutil.copytree(here,out/'frozen-inputs',ignore=shutil.ignore_patterns('__pycache__'))
        revision=run('revision',['git','-C',repo,'rev-parse',a.revision]).decode().strip()
        for directory,expected in [('src',SOURCE),('tests',TESTS)]:require(run(directory+'-tree',['git','-C',repo,'rev-parse',revision+':'+directory]).decode().strip()==expected,'tree mismatch')
        affinity=sorted(os.sched_getaffinity(0));require(affinity,'no CPU available');os.sched_setaffinity(0,{affinity[0]})
        require(shutil.disk_usage(out).free>4*1024**3,'less than4GiB free')
        metadata={'revision':revision,'source':SOURCE,'tests':TESTS,'candidate':CANDIDATE,'unit_flags':{u:compile_flags(u) for u in ['Engine','FactoryPresets','Ig00156Batch','driver']},'affinity_available':affinity,'affinity_selected':sorted(os.sched_getaffinity(0)),'uname':list(platform.uname()),'thread_clock_resolution':time.clock_getres(time.CLOCK_THREAD_CPUTIME_ID),'wall_clock_resolution':time.clock_getres(time.CLOCK_MONOTONIC),'environment':{k:os.environ.get(k) for k in ['ImageOS','ImageVersion','RUNNER_ARCH','RUNNER_ENVIRONMENT','GITHUB_SHA','GITHUB_RUN_ID','GITHUB_RUN_NUMBER','GITHUB_RUN_ATTEMPT']},'sanitizer_scope':'Prior local GNU14 O1 address/undefined only, leaks unqualified; not a GNU13 sanitized build'}
        require(metadata['thread_clock_resolution']>0 and metadata['wall_clock_resolution']>0,'invalid clock resolution')
        (out/'cpuinfo.txt').write_text(Path('/proc/cpuinfo').read_text());(out/'os-release.txt').write_text(Path('/etc/os-release').read_text());run('lscpu',['lscpu','-J'])
        compiler=Path(shutil.which('g++-13') or '').resolve();require(compiler.is_file(),'GNU13 unavailable')
        metadata['compiler_version']=run('compiler-version',[compiler,'--version']).decode();frontend=Path(run('compiler-frontend',[compiler,'-print-prog-name=cc1plus']).decode().strip()).resolve();require(frontend.is_file(),'frontend unavailable')
        bindings[str(compiler)]=sha(compiler);bindings[str(frontend)]=sha(frontend);write(out/'metadata.json',metadata)
        plan=schedule();require(len(plan)==84,'schedule size');write(out/'schedule.json',plan);bindings[str(out/'schedule.json')]=sha(out/'schedule.json')
        (out/'stock').mkdir();archive=run('source-archive',['git','-C',repo,'archive',revision,'src','tests']);run('source-extract',['tar','-x','-C',out/'stock'],stdin=archive)
        shutil.copytree(out/'stock/src',out/'candidate/src');run('candidate-patch',['patch','--batch','--fuzz=0','-p1'],cwd=out/'candidate',stdin=(here/'candidate.patch').read_bytes())
        stock_sources=inventory(out/'stock/src');candidate_sources=inventory(out/'candidate/src')
        require(stock_sources.keys()==candidate_sources.keys(),'candidate source inventory changed')
        require([k for k in stock_sources if stock_sources[k]!=candidate_sources[k]]==['core/Ig00156Batch.cpp'],'patch outside Ig backend')
        require(candidate_sources['core/Ig00156Batch.cpp']==CANDIDATE,'candidate variant mismatch')
        write(out/'source-manifests.json',{'stock':inventory(out/'stock'),'candidate':inventory(out/'candidate')})
        write(out/'source-contract.json',analysis.verify_source_contract(here/'driver.cpp',out/'stock/src/tools/source_factory_benchmark.cpp',out/'stock/src/core/Params.h',out/'candidate/src/core/Params.h'))
        common=out/'build/common';common.mkdir(parents=True);objects={};binaries={};links={}
        common_sources={'Engine':out/'stock/src/core/Engine.cpp','FactoryPresets':out/'stock/src/core/FactoryPresets.cpp','driver':here/'driver.cpp','batch-tests':out/'stock/tests/Ig00156BatchTests.cpp','engine-tests':out/'stock/tests/FilterBatchEngineTests.cpp'}
        for name,source in common_sources.items():
            obj=common/(name+'.o');run('compile-common-'+name,[compiler,*FLAGS,'-I'+str(out/'stock/src'),'-c',source,'-o',obj]);objects[name]=obj;bindings[str(obj)]=sha(obj)
        for arm in ['stock','candidate']:
            build=out/'build'/arm;build.mkdir();source=out/arm/'src/core/Ig00156Batch.cpp';ig=build/'Ig00156Batch.o'
            run('compile-'+arm+'-Ig00156Batch',[compiler,*compile_flags('Ig00156Batch'),'-I'+str(out/'stock/src'),'-c',source,'-o',ig])
            for name in ['driver','batch-tests','engine-tests']:
                used=([objects['Engine'],objects['FactoryPresets']] if name!='batch-tests' else [])+[objects[name],ig]
                binary=build/name;run('link-'+arm+'-'+name,[compiler,*FLAGS,*used,'-o',binary]);binaries[arm,name]=binary;links[arm+'-'+name]={'binary':str(binary),'sha256':sha(binary),'objects':{str(p):sha(p) for p in used}}
            # Adversarial test-only access includes the actual backend source in its own TU.
            component=build/'component';run('compile-'+arm+'-component',[compiler,*compile_flags('Ig00156Batch'),'-I'+str(out/'stock/src'),'-I'+str(out/'stock/src/core'),'-DBATCH_SOURCE="'+str(source)+'"',here/'component.cpp','-o',component]);binaries[arm,'component']=component
        validate_shared_links(links)
        require(inventory(out/'stock/src')==stock_sources and inventory(out/'candidate/src')==candidate_sources,'source changed during build')
        verify_bindings()
        for directory in [out/'build',out/'stock',out/'candidate']:
            for p in directory.rglob('*'):
                if p.is_file():bindings[str(p)]=sha(p)
        write(out/'bindings.json',bindings);write(out/'binary-arm-links.json',links)
        def render(label,argv,*,limit=600):
            require(sum(p.stat().st_size for p in out.rglob('*') if p.is_file())<1024**3,'1GiB artifact budget exceeded; retain all evidence');require(shutil.disk_usage(out).free>1024**3,'free disk below1GiB')
            verify_bindings();require(os.sched_getaffinity(0)=={affinity[0]},'affinity changed');result=run(label,argv,limit=limit);verify_bindings();return result
        for arm in ['stock','candidate']:
            render('component-'+arm,[binaries[arm,'component'],out/(arm+'-component.bin')],limit=120)
            render('batch-tests-'+arm,[binaries[arm,'batch-tests']],limit=120)
            render('engine-tests-'+arm,[binaries[arm,'engine-tests'],'--write-corpus',out/(arm+'-engine.corpus')],limit=180)
        for name in ['component.bin','engine.corpus']:require(sha(out/('stock-'+name))==sha(out/('candidate-'+name)),'qualification corpus mismatch: '+name)
        dry={};backend=None;input_hashes={}
        for row in range(7):
            for arm in ['stock','candidate']:
                dest=out/f'dry-{arm}-{row}';render(f'dry-{arm}-{row}',[binaries[arm,'driver'],'dry',arm,str(row),dest])
                receipt,digest=analysis.validate_receipt(dest,{'arm':arm,'row':row},root=out,phase='dry')
                require(backend is None or receipt['batch_kernel']==backend,'backend changed');backend=receipt['batch_kernel'];dry[arm,row]=digest
                inputs=tuple(sha(dest/n) for n in ['events.csv','parameters.csv']);require(row not in input_hashes or inputs==input_hashes[row],'dry input tables differ');input_hashes[row]=inputs
                if arm=='candidate':require(dry['stock',row]==digest,'dry stock/candidate PCM differs')
                canonicalize(dest,out)
        write(out/'dry-admission.json',{'pcm':{f'{arm}-{row}':h for (arm,row),h in dry.items()},'batch_kernel':backend})
        paths=[]
        for item in plan:
            arm=item['arm'];row=item['row'];dest=out/f'run-{item["index"]:03d}'
            render(f'{item["phase"]}-{item["index"]:03d}',[binaries[arm,'driver'],'timed',arm,str(row),dest])
            receipt,digest=analysis.validate_receipt(dest,item,root=out)
            require(receipt['batch_kernel']==backend,'timed backend changed');require(digest==dry[arm,row],'timed/dry PCM differs')
            require(tuple(sha(dest/n) for n in ['events.csv','parameters.csv'])==input_hashes[row],'timed input changed');canonicalize(dest,out);paths.append(dest)
            if len(paths)%42==0:
                result=analysis.validate_completed_prefix(paths,root=out);write(out/(item['phase']+'-analysis.json'),result)
                if item['phase']=='aa':require(result['decision']['decision']=='AA_PASS_ONLY',result['decision']['decision'])
                else:status=result['decision']
        verify_bindings()
    except BaseException as e:status={'decision':'STOP_NO_RETRY','reason':str(e),'exception':type(e).__name__}
    finally:signal.alarm(0);status['elapsed_wall_seconds']=time.monotonic()-start;write(out/'terminal.json',status);write(out/'evidence-manifest.json',inventory(out))
    return 1 if status['decision'].startswith('STOP') else 0
if __name__=='__main__':sys.exit(main())
