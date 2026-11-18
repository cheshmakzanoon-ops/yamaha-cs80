#!/usr/bin/env python3
"""Offline integrity and exact in-memory source reconstruction. Never runs DSP."""
import argparse,hashlib,json,re,sys
from pathlib import Path,PurePosixPath
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parent
def need(value, message):
    if not value:
        raise ValueError(message)

def sha(data):
    return hashlib.sha256(data).hexdigest()

def pairs(items):
    out = {}
    for k, v in items:
        need(k not in out, 'duplicate JSON key: ' + k)
        out[k] = v
    return out

def decode(data):
    return json.loads(data, object_pairs_hook=pairs)

def safe(name):
    p = PurePosixPath(name)
    need(name and (not p.is_absolute()) and ('..' not in p.parts) and (str(p) == name) and ('\\' not in name), 'unsafe path')
    return name

def hash_tree(files):
    return {safe(k): sha(v) for k, v in files.items()}

def check_inventory(files, expected):
    need(hash_tree(files) == expected, 'source inventory mismatch')

def read_tree(root):
    need(root.is_dir() and (not root.is_symlink()), 'expected real directory')
    result = {}
    for p in root.rglob('*'):
        need(not p.is_symlink(), 'symlinks are not accepted')
        if p.is_file():
            result[safe(p.relative_to(root).as_posix())] = p.read_bytes()
    return result

def apply_patch(files, patch):
    """Strict byte-preserving subset used by the frozen unified patch. No fuzz."""
    out = dict(files)
    lines = patch.splitlines(keepends=True)
    i = 0
    seen = set()
    while i < len(lines):
        need(lines[i].startswith(b'--- '), 'expected old patch header')
        old = lines[i][4:].rstrip(b'\n').decode()
        i += 1
        need(i < len(lines) and lines[i].startswith(b'+++ '), 'expected new patch header')
        new = lines[i][4:].rstrip(b'\n').decode()
        i += 1
        need(new.startswith('b/src/'), 'new path prefix')
        name = safe(new[6:])
        need(name not in seen, 'duplicate patch file')
        seen.add(name)
        if old == '/dev/null':
            need(name not in out, 'new file already exists')
            source = []
        else:
            need(old == 'a/src/' + name and name in out, 'old path/file mismatch')
            source = out[name].splitlines(keepends=True)
        cursor = 0
        result = []
        hunks = 0
        while i < len(lines) and lines[i].startswith(b'@@ '):
            m = re.fullmatch(b'@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@[^\\n]*\\n', lines[i])
            need(m is not None, 'hunk header')
            i += 1
            hunks += 1
            start, n, end, k = [int(x) if x is not None else 1 for x in m.groups()]
            pos = start - 1 if n else start
            need(cursor <= pos <= len(source), 'hunk old offset')
            result.extend(source[cursor:pos])
            cursor = pos
            need(len(result) == (end - 1 if k else end), 'hunk new offset')
            used = made = 0
            while used < n or made < k:
                need(i < len(lines), 'truncated hunk')
                line = lines[i]
                i += 1
                need(line[:1] in (b' ', b'+', b'-'), 'hunk operation')
                if line[:1] != b'+':
                    need(cursor < len(source) and source[cursor] == line[1:], 'hunk context mismatch')
                    cursor += 1
                    used += 1
                if line[:1] != b'-':
                    result.append(line[1:])
                    made += 1
                need(used <= n and made <= k, 'hunk line count')
        need(hunks > 0, 'empty patch')
        result.extend(source[cursor:])
        out[name] = b''.join(result)
    return out

def records(d):
 p=d['provenance.json'];s=d['source-inventory.json'];h=d['records/held.json'];e=d['records/exceptional.json'];t=d['records/static-v2.json'];c=d['records/scenes.json'];o=d['records/stock-opportunity.json']
 need(p['baseline_commit']==s['baseline_commit']=='e9e8175f4278d2457279fcb8809cbba1ddb7be2b','baseline')
 need(p['actual_recorded_patch_sha256']==s['patch_sha256'],'patch provenance')
 need(p['normal_sha256']['libcs80_core.a']=='ae34d6a4899a03fdbddf204520ea01644b77f379a38e611b84bdaedfcd689005','normal archive')
 need(p['component_cpp_sha256']==s['candidate_sha256']['core/Ig00156Eight.cpp']=='3487dbc28f491f0c53f5d42871e2c59ddb87b1839db2aaddd1bf82c573adb8a1','component freeze')
 need(p['fresh_engine_invocations']==26 and p['cpu_timing_invocations']==p['native_module_invocations']==0 and not p['normal_reporting_hooks'],'scope')
 need(len(s['candidate_sha256'])==s['candidate_files']==196 and s['stock_files']==193,'source roster')
 need(t['critical']['passed'] and not any(t['critical']['extra_protected_definitions'].values()),'static protected gate')
 for x,size in zip(t['critical']['critical'],[32154,37511,12063]):need(x['stock_bytes']==x['candidate_bytes']==size and x['bytes_identical'] and x['resolved_symbolic_relocations_identical'],'protected identity')
 need(t['shared_weak_count']==14 and t['equal_byte_shared_weak_count']==13 and not t['arbitrary_link_safety_proved'],'weak limits')
 need(h['passed'] and h['bytes_per_arm']==24109781 and h['arms']==['stock','normal','observed'],'held parity')
 need('window attempts=8223 hits=8067 consumed=8067 bad=0' in h['observer'],'held actual reach')
 need(len(c['rows'])==16 and all(x['stock_equal']for x in c['rows']),'four scene comparisons')
 for name,size in [('factory-moving',24489391),('panel-moving',24492625),('pure-held',24027930),('sparse-three',24008449)]:
  rows=[x for x in c['rows']if x['scene']==name];need({x['arm']for x in rows}=={'stock','normal','reverse','observed'}and len({x['sha256']for x in rows})==1 and all(x['bytes']==size for x in rows),'scene coverage '+name)
  obs=next(x for x in rows if x['arm']=='observed')['stdout'];need(('window attempts=345 hits=328 consumed=328 bad=0'if name=='factory-moving'else'window attempts=0 hits=0 consumed=0 bad=0')in obs,'actual scene hit')
 need(e['trap_cases']==22 and e['real_SIGFPE']==7 and len(e['normalized_signals'])==22 and sum(bool(x[1])for x in e['normalized_signals'])==7,'real traps')
 need(e['equal_signal_arms']==['stock','normal','reverse','observed'],'signal arms')
 for x in e['trap_rows']:need(x['bytes']==17019552 and x['stock_equal'] and x['normalized_signal_equal'],'trap parity')
 need(e['observed_gate']['stock_equal'] and e['observed_gate']['nine_unsupported_fp_target_samples_zero_attempts'],'observed unsupported gate')
 for x in e['directed_rows']:need(x['bytes']==64267367 and x['stock_equal'],'directed parity')
 need((e['directed_cases'],e['directed_callbacks'],e['idle_callbacks'],e['enabled_bbd_callbacks'])==(16,2064,768,1032),'directed reach')
 need(len(o['rows'])==4 and all(x['counts']['pairs']==8192 and x['counts']['distinct_eight']==8192 for x in o['rows']),'stock opportunity')
 pure=next(x for x in o['rows']if x['scene']=='pure-held');need(pure['counts']['both_accepted']==8053,'Pure stock eligibility differs from zero integration')
 need(d['records/rejected-v1.json']['candidate_Engine_renders']==0,'V1 never rendered')

def verify_package(root=ROOT):
 f=read_tree(root);m=decode(f['manifest.json']);need(m['schema']==1,'manifest');need(set(f)==set(m['files'])|{'manifest.json'},'package set')
 for name,x in m['files'].items():need(len(f[name])==x['bytes']and sha(f[name])==x['sha256'],'digest '+name)
 d={n:decode(x)for n,x in f.items()if n.endswith('.json')and n!='manifest.json'};records(d)
 need(sha(f['candidate.patch'])==d['provenance.json']['actual_recorded_patch_sha256'],'actual patch binding')
 need(sha(f['observer.patch'])==d['provenance.json']['observer_patch_sha256'],'observer patch')
 return f,d

def reconstruct(stock,inv,patch):
 expected=dict(inv['candidate_sha256'])
 for name,x in inv['stock_overrides'].items():
  if x is None:expected.pop(name)
  else:expected[name]=x
 need(len(expected)==193,'stock files');check_inventory(stock,expected);candidate=apply_patch(stock,patch);check_inventory(candidate,inv['candidate_sha256']);return candidate
if __name__=='__main__':
 ap=argparse.ArgumentParser(description=__doc__);ap.add_argument('--stock-src',type=Path);a=ap.parse_args()
 try:
  files,docs=verify_package()
  if a.stock_src:
   candidate=reconstruct(read_tree(a.stock_src),docs['source-inventory.json'],files['candidate.patch']);observed=apply_patch(candidate,files['observer.patch']);need(sha(observed['core/EngineFilterEight.cpp'])==docs['provenance.json']['observer_source_sha256'],'actual observer reconstruction');print('PASS 193 stock ->196 exact candidate files, exact observer; no DSP.')
  else:print('PASS package/receipt integrity only; no source reconstruction or DSP.')
 except (ValueError,KeyError,TypeError,OSError)as e:print('FAIL:',e,file=sys.stderr);sys.exit(1)
