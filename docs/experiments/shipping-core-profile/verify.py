"""Replay saved PC counts; optionally authenticate supplied ELFs. Never runs DSP."""
import argparse, collections, hashlib, json, math, re, sys
from pathlib import Path
sys.dont_write_bytecode = True
from pins import PINS
from elf_evidence import Elf, digest, dwarf

ROOT = Path(__file__).resolve().parent
INPUTS = tuple(PINS['inputs'])
ORDER = [('factory-held',1999),('factory-moving',5003),('panel-moving',1999),
         ('factory-held',5003),('factory-moving',1999),('panel-moving',5003)]
LIBRARY_OWNER = '[libm: unattributed library/self PC]'

def require(ok, message):
    if not ok:
        raise ValueError(message)

def integer(x, minimum=0):
    return type(x) is int and x >= minimum

def covered(module, address):
    return any(int(lo,16) <= address < int(hi,16) for _,lo,hi in module['executable_ranges'])

def exclusive_owner(module, physical, chain):
    # Exactly one owner per PC. Inline parents are NOT additional samples.
    if module == 'libm':
        return LIBRARY_OWNER
    functions = [f for f,s in chain]
    if any('cs80::Ig00156Stage::set(' in f for f in functions):
        return 'Ig00156Stage::set including inline descendants (NOT external libm)'
    if 'ReconstructedOscillator::' in physical or any('ReconstructedOscillator::' in f for f in functions):
        return 'ReconstructedOscillator family'
    if 'Ig00156BatchAccess::process(' in physical:
        return 'Ig00156Batch process'
    if 'updateSourceReadouts(' in physical:
        return 'Engine::updateSourceReadouts'
    if 'PraWahCircuit::process(' in physical:
        return 'PraWahCircuit::process'
    return physical

def load(root=ROOT):
    return {name:json.loads((root/name).read_text()) for name in INPUTS}

def check_content_anchors(root=ROOT):
    for name, expected in PINS['inputs'].items():
        actual = hashlib.sha256((root/name).read_bytes()).hexdigest()
        require(actual == expected, 'content anchor mismatch: '+name)
    identity=json.loads((root/'build-identity.json').read_text())
    for key,name in [('method_source_sha256','profile.cpp'),('exported_collector_sha256','collect.py'),('exported_plan_sha256','PLAN.txt')]:
        require(hashlib.sha256((root/name).read_bytes()).hexdigest()==identity[key], 'method source identity mismatch')

def check_manifest(root=ROOT):
    manifest = json.loads((root/'manifest.json').read_text())
    actual = {p.relative_to(root).as_posix() for p in root.rglob('*') if p.is_file()}
    require(actual == set(manifest)|{'manifest.json'}, 'unexpected or missing bundle file')
    for name, expected in manifest.items():
        require(not name.startswith('/') and '..' not in Path(name).parts, 'unsafe manifest member')
        require(hashlib.sha256((root/name).read_bytes()).hexdigest() == expected, 'manifest mismatch: '+name)

def replay(bundle):
    m = bundle['modules.json']; s = bundle['symbols.json']; p = bundle['pc-counts.json']
    c = bundle['captures.json']; identity = bundle['build-identity.json']
    require(m['schema']=='pinned-elf-modules-v1' and s['schema']=='deduplicated-physical-and-inline-source-v1', 'module/symbol schema')
    require(p['schema']=='module-relative-counted-pcs-v1' and c['schema']=='stock-sampling-capture-metadata-v1', 'PC/capture schema')
    require(identity['schema']=='stock-pic-debug-identity-receipt-v1', 'identity schema')
    require(set(m['modules']) == {'main','libm'}, 'exact module roster')
    for label, module in m['modules'].items():
        require(re.fullmatch('[0-9a-f]{64}',module.get('sha256','')) is not None, 'module SHA required')
        require(module['sha256']==PINS['modules'][label], 'pinned module SHA mismatch')
        require(integer(module['bytes'],1) and module['elf_type']==3, 'module identity type/size')
        ranges = module['executable_ranges']
        require(bool(ranges) and len({n for n,lo,hi in ranges})==len(ranges), 'executable range roster')
        intervals = sorted((int(lo,16),int(hi,16)) for n,lo,hi in ranges)
        require(all(0<=lo<hi for lo,hi in intervals), 'executable interval')
        require(all(a[1]<=b[0] for a,b in zip(intervals,intervals[1:])), 'overlapping executable intervals')
    require(identity['linked_pair']['debug_sha256']==PINS['modules']['main'], 'identity/main mismatch')
    require(set(identity['objects'])=={'Engine','FactoryPresets','Ig00156Batch'}, 'core object roster')
    for obj in identity['objects'].values():
        require(obj['all_allocatable_bytes_and_relocations_identical'] is True, 'object identity receipt')
        for key in ('normal_sha256','debug_sha256','allocatable_sections_digest','normalized_allocatable_relocations_digest'):
            require(re.fullmatch('[0-9a-f]{64}',obj[key]) is not None, 'object identity digest')
    require(identity['linked_pair']['identical_remaining_allocatable_sections_and_addresses'] is True, 'linked identity receipt')
    require(identity['linked_pair']['excluded_section']=='.note.gnu.build-id', 'identity exception scope')

    functions, frames, chains = s['functions'],s['frames'],s['chains']
    require(len(functions)==len(set(functions)) and all(isinstance(f,str) and f for f in functions), 'function dictionary')
    for fid, source in frames:
        require(integer(fid) and fid<len(functions), 'frame function index')
        require(isinstance(source,str) and source.startswith(('project/','toolchain/','??:')), 'portable source category')
        require('/workspace/' not in source and '/home/' not in source, 'private source path')
    for chain in chains:
        require(0<len(chain)<=32 and all(integer(i) and i<len(frames) for i in chain), 'inline-chain coverage')
    annotations=[]; seen=set()
    for row in p['pcs']:
        require(len(row)==4, 'PC record arity')
        label, offset, physical_id, chain_id = row
        require(label in m['modules'], 'unknown PC module')
        require(isinstance(offset,str) and re.fullmatch('0x[0-9a-f]+',offset), 'canonical module-relative PC')
        address=int(offset,16);require((label,address) not in seen, 'duplicate normalized PC');seen.add((label,address))
        require(covered(m['modules'][label],address), 'PC outside executable coverage')
        if label=='libm':
            require(physical_id is None and chain_id is None, 'library caller attribution forbidden')
            annotations.append((label,address,LIBRARY_OWNER,[]))
        else:
            require(integer(physical_id) and physical_id<len(functions), 'physical annotation coverage')
            require(integer(chain_id) and chain_id<len(chains), 'DWARF annotation coverage')
            chain=[[functions[frames[i][0]],frames[i][1]] for i in chains[chain_id]]
            annotations.append((label,address,functions[physical_id],chain))
    require(len(c['captures'])==len(p['captures'])==6, 'six fixed captures required')
    calibration=m['calibration_loop'];require(len(calibration)==3 and calibration[0]=='main', 'calibration module')
    cal_lo,cal_hi=int(calibration[1],16),int(calibration[2],16)
    require(cal_lo<cal_hi and covered(m['modules']['main'],cal_lo) and covered(m['modules']['main'],cal_hi-1), 'calibration range')
    used=set(); reports=[]; prefixes={}
    for slot,(metadata,counted,expected) in enumerate(zip(c['captures'],p['captures'],ORDER),1):
        require(metadata['slot']==counted['slot']==slot, 'slot identity/order')
        require((metadata['scene'],metadata['period_us'])==expected, 'predeclared scene/period order')
        require(metadata['overflow']==0 and metadata['clock']=='CLOCK_PROCESS_CPUTIME_ID', 'capture status/clock')
        require(integer(metadata['samples'],1000) and integer(metadata['outside_core_samples']), 'recorded sample-count domain')
        require(integer(metadata['blocks'],4096) and metadata['frames']==64*metadata['blocks'], 'frame/block conservation')
        require(metadata['voice_checks']==8*metadata['blocks'], 'eight-voice check conservation')
        require(integer(metadata['nonzero'],1) and metadata['nonzero']<=2*metadata['frames'], 'recorded capture has nonzero PCM signal')
        require(math.isfinite(metadata['sum_squares']) and metadata['sum_squares']>0, 'finite nonzero PCM receipt')
        require(20_000_000_000<=metadata['cpu_ns']<21_000_000_000, 'bounded CPU-window receipt')
        require(metadata['oversampling']==2 and metadata['routes']==([1,1] if metadata['scene']=='panel-moving' else [4,4]), 'recorded source routes')
        require(metadata['mxcsr_start']==metadata['mxcsr_end'], 'FP environment receipt')
        for key in ('pcm_hash','prefix4096_hash'):
            require(re.fullmatch('[0-9a-f]{16}',metadata[key]) is not None, 'PCM fingerprint format')
        previous=prefixes.setdefault(metadata['scene'],metadata['prefix4096_hash'])
        require(previous==metadata['prefix4096_hash'], 'matching fixed-prefix PCM receipts')
        owners=collections.Counter();physical=collections.Counter();indices=set();total=0
        for index,count in counted['counts']:
            require(integer(index) and index<len(annotations) and index not in indices, 'unique counted PC index')
            require(integer(count,1), 'positive sample count')
            indices.add(index);used.add(index);label,address,fn,chain=annotations[index]
            owners[exclusive_owner(label,fn,chain)]+=count;physical[fn]+=count;total+=count
        require(total==metadata['samples'] and sum(owners.values())==total, 'sample/owner count conservation')
        require(dict(owners)==metadata['exclusive_owner_counts'], 'exclusive ownership replay mismatch')
        require(dict(physical)==metadata['physical_function_counts'], 'physical attribution replay mismatch')
        caltotal=0; seen_cal=set()
        for offset,count in counted['calibration_counts']:
            address=int(offset,16)
            require(address not in seen_cal and cal_lo<=address<cal_hi and integer(count,1), 'calibration PC evidence')
            seen_cal.add(address);caltotal+=count
        require(caltotal==metadata['calibration_samples'] and caltotal>=8, 'calibration count conservation')
        reports.append({'slot':slot,'scene':metadata['scene'],'period_us':metadata['period_us'],
                        'samples':total,'calibration_samples':caltotal,'exclusive_owners':dict(owners)})
    require(used==set(range(len(annotations))), 'every exported PC must have capture coverage')
    require(sum(r['samples'] for r in reports)==25613, 'fixed dataset aggregate sample count')
    require(sum(r['outside_core_samples'] for r in c['captures'])==36, 'outside-core count conservation')
    return {'mode':'receipt-and-count-replay','module_bytes_verified':False,
            'causal_performance_claim':False,'captures':6,'core_samples':25613,'outside_core_samples':36,
            'normalized_pc_count':len(annotations),'per_capture':reports}

def authenticate_modules(bundle, paths):
    require(set(paths)=={'main','libm'}, 'both exact sampled module files required')
    modules=bundle['modules.json']['modules']; actual={}
    for label,path in paths.items():
        e=Elf(path);actual[label]=e
        require(e.sha256==PINS['modules'][label], 'supplied module SHA mismatch: '+label)
        require(e.executable_ranges()==modules[label]['executable_ranges'], 'supplied ELF coverage mismatch')
    s=bundle['symbols.json'];pcs=bundle['pc-counts.json']['pcs'];lookup,names=actual['main'].physical_map()
    mainpcs={int(o,16) for m,o,f,c in pcs if m=='main'};lines=dwarf(paths['main'],mainpcs)
    for module,offset,fid,cid in pcs:
        if module!='main':continue
        addr=int(offset,16);require(lookup(addr)==s['functions'][fid], 'ELF physical symbol mismatch')
        expected=[[s['functions'][s['frames'][i][0]],s['frames'][i][1]] for i in s['chains'][cid]]
        require(lines[addr]==expected, 'DWARF inline/source mapping mismatch')
    cal=[k for k,n in names.items() if 'calibrationLoop' in n]
    require(len(cal)==1 and bundle['modules.json']['calibration_loop']==['main',hex(cal[0][0]),hex(sum(cal[0]))], 'ELF calibration function mismatch')
    return {'module_bytes_verified':True,'module_sha256':dict(PINS['modules']),
            'physical_symbols_verified':True,'dwarf_source_mapping_verified':True,'main_pcs_checked':len(mainpcs)}

def authenticate_build(bundle, root):
    receipt=bundle['build-identity.json'];root=Path(root)
    for name,row in receipt['objects'].items():
        a=Elf(root/(name+'.normal.o'));b=Elf(root/(name+'.debug.o'))
        require(a.sha256==row['normal_sha256'] and b.sha256==row['debug_sha256'], 'supplied object SHA mismatch')
        require(a.allocatable()==b.allocatable(), 'object allocatable code/data changed')
        require(a.relocations()==b.relocations(), 'object normalized relocation changed')
        require(digest(a.allocatable())==row['allocatable_sections_digest'], 'object section receipt mismatch')
        require(digest(a.relocations())==row['normalized_allocatable_relocations_digest'], 'object relocation receipt mismatch')
    a=Elf(root/'profile-stock');b=Elf(root/'profile-debug');r=receipt['linked_pair']
    require(a.sha256==r['normal_sha256'] and b.sha256==r['debug_sha256'], 'linked pair SHA mismatch')
    aa=a.allocatable();bb=b.allocatable();aa.pop('.note.gnu.build-id');bb.pop('.note.gnu.build-id')
    require(aa==bb and digest(aa)==r['remaining_allocatable_digest'], 'linked code/address identity mismatch')
    return {'supplied_build_identity_verified':True,'core_object_pairs':3,'linked_pair_code_and_addresses_identical':True}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--replay',action='store_true',help='verify the portable recorded evidence without supplying binaries')
    parser.add_argument('--module',action='append',default=[],metavar='LABEL=FILE',help='supply BOTH main and libm exact modules; verifies SHA, ELF ranges, symbols and DWARF')
    parser.add_argument('--identity-root',type=Path,help='optional exact normal/debug objects and linked pair; naming described in README')
    args=parser.parse_args();require(args.replay or args.module,'choose --replay or supply exact --module files')
    check_manifest();check_content_anchors();bundle=load();result=replay(bundle)
    if args.module:
        pairs=[x.split('=',1) for x in args.module];require(all(len(x)==2 for x in pairs),'module argument format')
        require(len({x[0] for x in pairs})==len(pairs),'duplicate module argument')
        result.update(authenticate_modules(bundle,dict(pairs)))
        result['mode']='exact-module-authenticated-replay'
    if args.identity_root:result.update(authenticate_build(bundle,args.identity_root))
    print(json.dumps(result,sort_keys=True,indent=2))
if __name__=='__main__':
    try:main()
    except (ValueError,AssertionError,KeyError,TypeError,IndexError) as error:
        print('VERIFICATION FAILED: '+str(error),file=sys.stderr);sys.exit(1)
