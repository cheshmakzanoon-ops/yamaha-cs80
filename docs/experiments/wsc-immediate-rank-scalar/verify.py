#!/usr/bin/env python3
"""Offline package/receipt checks; optional strict source reconstruction. No DSP."""
import argparse
import hashlib
import json
import re
import sys
from pathlib import Path, PurePosixPath
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parent
BASE = 'e9e8175f4278d2457279fcb8809cbba1ddb7be2b'
PATCH = '53946c97a462851fcec156a0afe94164bc3a4fa1233b39b6fffda4f5e7f8cecf'
CPP = '0beda1833b75bc9986abb958063954e73576ffe2d7a07afa90b58de41f206cf8'
ARCHIVE = 'eeba3a3b635b05f8f7eb797564a26511ce738291acf6c60b5d5ba915e79869a0'
SCENES = {'factory_held','factory_moving','panel_held','panel_moving','pure_held',
          'legacy_held','factory_quiet','partial_three','release_wake'}
def need(value, message):
    if not value:
        raise ValueError(message)
def sha(data):
    return hashlib.sha256(data).hexdigest()
def pairs(items):
    out = {}
    for k, v in items:
        need(k not in out, 'duplicate JSON key: '+k)
        out[k] = v
    return out
def decode(data):
    return json.loads(data, object_pairs_hook=pairs)
def safe(name):
    p = PurePosixPath(name)
    need(name and not p.is_absolute() and '..' not in p.parts and
         str(p) == name and '\\' not in name, 'unsafe path')
    return name
def hash_tree(files):
    return {safe(k):sha(v) for k,v in files.items()}
def check_inventory(files, expected):
    need(hash_tree(files) == expected, 'source inventory mismatch')
def read_tree(root):
    need(root.is_dir() and not root.is_symlink(), 'expected real directory')
    result = {}
    for p in root.rglob('*'):
        need(not p.is_symlink(), 'symlinks are not accepted')
        if p.is_file():
            result[safe(p.relative_to(root).as_posix())] = p.read_bytes()
    return result
def check_bundle(files):
    need('manifest.json' in files, 'missing manifest')
    m = decode(files['manifest.json'])
    need(m['schema'] == 1 and m['scope'] == 'optional correctness checkpoint only', 'manifest scope')
    expected = m['files']
    need(set(files) == set(expected)|{'manifest.json'}, 'package file set')
    for name, entry in expected.items():
        safe(name)
        need(entry['bytes'] == len(files[name]) and entry['sha256'] == sha(files[name]), 'package digest: '+name)
    need(sum(len(v) for v in files.values()) <= 150000, 'lean package limit')
    docs = {k:decode(v) for k,v in files.items() if k.endswith('.json') and k != 'manifest.json'}
    check_records(docs)
    need(sha(files['candidate.patch']) == PATCH, 'candidate patch binding')
    return docs
def check_records(d):
    p=d['provenance.json']; s=d['source-inventory.json']
    e=d['records/engine.json']; c=d['records/direct.json']
    a=d['records/independent.json']; f=d['records/fixture.json']; w=d['records/work-count.json']
    need(p['arm']=='C-immediate-one-rank-scalar', 'distinct C arm')
    need(p['baseline_commit']==s['baseline_commit']==BASE and p['patch_sha256']==s['patch_sha256']==PATCH, 'baseline/patch')
    need(p['component_cpp_sha256']==s['candidate_sha256']['core/WscImmediateScalar.cpp']==c['source_sha256']==CPP, 'component source')
    normal=p['normal_artifacts_sha256']
    need(normal['libcs80_core.a']==e['normal_archive_sha256']==f['normal_archive_sha256']==ARCHIVE, 'normal archive')
    need(set(normal)=={'Engine.cpp.o','EngineWscImmediate.cpp.o','WscImmediateScalar.cpp.o','FactoryPresets.cpp.o','Ig00156Batch.cpp.o','libcs80_core.a'}, 'complete normal archive')
    need(len(s['candidate_sha256'])==s['candidate_files']==196 and s['stock_files']==193, 'source count')
    changes={'core/Engine.h','core/Engine.cpp','core/ReconstructedOscillator.h','core/EngineWscImmediate.cpp','core/WscImmediateScalar.h','core/WscImmediateScalar.cpp'}
    need(set(s['stock_overrides'])==changes and sum(v is None for v in s['stock_overrides'].values())==3, 'source change set')
    for name, digest in s['candidate_sha256'].items():
        safe(name); need(re.fullmatch('[0-9a-f]{64}', digest), 'source digest syntax')
    need(a['reconstruction']['source_count']==196 and a['reconstruction']['all_reconstructed_sources_match'] is True, 'independent source reconstruction')
    need(a['reconstruction']['source_patch']==PATCH and a['reconstruction']['independently_built_objects_and_archive']==normal, 'independent full build')
    need(c['authentic_oracle_pass'] and c['reverse_link_pass'] and c['authentic_stock_tick_bytes']==12063, 'direct genuine oracle')
    cm=c['matrix']; need((cm['checks'],cm['counted_ticks'],cm['fp_pairs'])==(38333413,319444,88694), 'direct scope')
    need(cm['immediate_ticks']==61396 and cm['original_fallback_ticks']==258048 and cm['counted_ticks']==cm['immediate_ticks']+cm['original_fallback_ticks'], 'direct counts')
    need(c['actual_sigfpe']['seed_pairs']==5 and c['actual_sigfpe']['whole_object_bytes']==5984 and c['actual_sigfpe']['object_or_continuation_differences']==c['actual_sigfpe']['actual_pc_fp_errno_differences']==0, 'direct real traps')
    need(c['environment']['strict_admitted']==4 and c['environment']['fma_admitted']==c['environment']['unsafe_admitted']==c['environment']['fp_errno_changes']==0, 'strict caller controls')
    need(c['performance_timing'] is False and c['native_runs']==0, 'direct limits')
    ci=e['critical_isolation']; need(ci['passed'] and len(ci['critical'])==3, 'critical code gate')
    for row, size, reloc in zip(ci['critical'],[32154,37511,12063],[646,693,136]):
        need(row['stock_bytes']==row['candidate_bytes']==size and row['relocations']==reloc and row['bytes_identical'] and row['resolved_symbolic_relocations_identical'] and row['stock_sha256']==row['candidate_sha256'], 'original bytes/relocations')
    need(not any(ci['extra_protected_definitions'].values()), 'competing original definitions')
    need(set(e['scenes'])==SCENES and e['all_81_comparisons_exact'] and e['fixture']['callbacks_per_scene']==385, 'nine-scene gate')
    need(e['normal_has_hooks'] is False and e['timing'] is False, 'normal diagnostic isolation')
    totals=[0,0,0]
    for name, row in e['scenes'].items():
        streams=row['streams']; need(set(streams)=={'.trace.bin','.wsc.bin','.rng.bin'}, 'trace streams')
        for kind, v in streams.items():
            arms=['rng-stock','counted'] if kind=='.rng.bin' else ['stock','candidate','counted','rng-stock','reverse']
            need(v['equal_arms']==arms and re.fullmatch('[0-9a-f]{64}',v['sha256']), 'equal arm coverage')
        totals[0]+=streams['.trace.bin']['bytes']+streams['.wsc.bin']['bytes']
        totals[1]+=row['actual_rank_draws']; totals[2]+=streams['.rng.bin']['bytes']
        need(streams['.rng.bin']['bytes']==12*row['actual_rank_draws']+8*385, 'ordered RNG encoding')
        late=row['late']; calls=late['calls']
        need(calls==late['terminal_hits']==late['returns']==late['chronologically_following_rank_draws'] and late['bad']==row['all']['bad']==0, 'same-rank chronology')
        need(len(late['per_rank_calls'])==len(late['per_rank_terminal_hits'])==16 and sum(late['per_rank_calls'])==sum(late['per_rank_terminal_hits'])==calls, 'rank association')
        expected=0 if name in {'pure_held','legacy_held','factory_quiet'} else 98304 if name=='partial_three' else 41984 if name=='release_wake' else 262144
        need(calls==expected, 'late actual reach')
        if expected==0: need(row['all']['calls']==row['all']['terminal_hits']==0, 'negative all reach')
    need(totals==[e['public_private_fp_bytes_per_arm'],e['ordered_rank_draws'],e['ordered_rng_bytes']]==[63532546,5555888,66698376], 'whole Engine trace totals')
    seeds=e['seed_controls']; expected=[(32,32),(32,31)]+[(32,32)]*5+[(32,31),(0,0),(0,0),(0,0),(12,12)]
    need(len(seeds['cases'])==12, 'seed roster')
    for i,(row,counts) in enumerate(zip(seeds['cases'],expected)):
        need(row['case']==i and (row['calls'],row['terminal_hits'])==counts and row['bad']==0, 'real mixed/negative seed')
    need({r['arm'] for r in seeds['comparisons']}=={'candidate','counted','rng-stock','reverse'}, 'seed arms')
    for row in seeds['comparisons']:
        need(row['exact'] and row['bytes']==1094075 and row['stock_sha256']==row['candidate_sha256'], 'seed exact')
    need(seeds['ordered_rng']['exact'] and seeds['ordered_rng']['bytes']==22560, 'seed RNG')
    t=e['public_traps']; need(t['cases']==22 and t['real_sigfpe_cases']==7 and t['masked_continuation_frames']==4 and t['includes_precision_only_csr']=='0x0f80' and t['selected_original_tick_bytes']==12063, 'real public trap scope')
    need({r['arm'] for r in t['comparisons']}=={'stock-aa','candidate','reverse'}, 'public trap arms')
    for row in t['comparisons']:
        need(row['exact'] and row['normalized_signals_identical'] and row['bytes']==row['stock_bytes']==17019552 and row['stock_sha256']==row['candidate_sha256'], 'partial state/continuation')
    weak=a['weak_symbols']; need(len(weak)==16 and sum(r['same_bytes'] for r in weak)==14, 'shared weak audit')
    need(a['weak_disposition']['all_resolved_relocations_proven'] is False and a['weak_disposition']['arbitrary_link_safety_claimed'] is False, 'weak caveat')
    di=a['directed']; need(di['candidate_archive_sha256']==ARCHIVE and di['actual_bytes_identical'] and set(di['arms'])=={'stock','normal','reverse'}, 'directed arms')
    need(len({r['sha256'] for r in di['arms'].values()})==1 and all(r['bytes']==64267367 for r in di['arms'].values()), 'directed equality')
    need((a['directed_callbacks'],a['idle_callbacks'],a['enabled_bbd_callbacks'])==(2064,768,1032), 'directed reach')
    need(f['passed'] and f['invocations']==12 and f['stock_reexecutions']==f['timing_invocations']==f['native_loads']==f['profiles']==f['recorded_cpu_and_empty_clock_values']==0, 'untimed fixed fixture')
    need(f['normal_hooks'] is False and f['normal_archive_unchanged'], 'fixture normal isolation')
    need((f['sample_rate'],f['frames'],f['oversampling'],f['notes'],f['warm_blocks'],f['window_blocks'],f['continuation_blocks'])==(48000,64,2,8,1024,512,1) and f['frame_before_midi_every_block'], 'full fixture schedule')
    need(set(f['scenes'])=={'factory-held','factory-moving','panel-moving','pure-held'}, 'full fixture scenes')
    for name,row in f['scenes'].items():
        need(row['trace_bytes']==1438632 and row['full_trace_byte_equal_to_B'] and row['normal_counted_exact'], 'full fixture parity')
        need(row['baseline_trace_sha256']==row['diagnostic_trace_sha256']==row['counted_trace_sha256'] and row['normal_identity_sha256']==row['baseline_identity_sha256'], 'full fixture digests')
        counts=row['actual_counts']; need(counts['reset_calls']==counts['snapshot_calls']==1, 'window boundaries')
        factor=0 if name=='pure-held' else 1
        for key,val in [('warm',2097152),('window',1048576),('after_snapshot',2048)]:
            need(counts[key+'_entries']==counts[key+'_terminals']==val*factor, 'actual full-window counts')
    need(w['static']==c['structural'] and w['static']['endpoint_rematerialization_removed_bytes']==48 and w['static']['remaining_endpoint_scratch_bytes']==112 and w['static']['total_traffic_saving_claimed'] is False and w['static']['tailcall_prediction_succeeded'] is False, 'static limitation')
    need(w['C_performance_admitted'] is False and w['S_V_costs_attributed_to_C'] is False and w['bypass_attempted'] is False and w['tools_installed'] is False, 'performance scope')
    fe=w['feasibility']; need(fe['instruction_counts_collected'] is False and fe['timing_collected'] is False and fe['synth_invocations']==0 and fe['host_settings_changed'] is False, 'stopped work-count attempt')
def apply_patch(files, patch):
    """Strict byte-preserving subset used by the frozen unified patch. No fuzz."""
    out=dict(files); lines=patch.splitlines(keepends=True); i=0; seen=set()
    while i<len(lines):
        need(lines[i].startswith(b'--- '), 'expected old patch header')
        old=lines[i][4:].rstrip(b'\n').decode(); i+=1
        need(i<len(lines) and lines[i].startswith(b'+++ '), 'expected new patch header')
        new=lines[i][4:].rstrip(b'\n').decode(); i+=1
        need(new.startswith('b/src/'), 'new path prefix'); name=safe(new[6:])
        need(name not in seen, 'duplicate patch file'); seen.add(name)
        if old=='/dev/null':
            need(name not in out, 'new file already exists'); source=[]
        else:
            need(old=='a/src/'+name and name in out, 'old path/file mismatch')
            source=out[name].splitlines(keepends=True)
        cursor=0; result=[]; hunks=0
        while i<len(lines) and lines[i].startswith(b'@@ '):
            m=re.fullmatch(rb'@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@[^\n]*\n',lines[i])
            need(m is not None, 'hunk header'); i+=1; hunks+=1
            start,n,end,k=[int(x) if x is not None else 1 for x in m.groups()]
            pos=start-1 if n else start
            need(cursor<=pos<=len(source), 'hunk old offset')
            result.extend(source[cursor:pos]); cursor=pos
            need(len(result)==(end-1 if k else end), 'hunk new offset')
            used=made=0
            while used<n or made<k:
                need(i<len(lines), 'truncated hunk'); line=lines[i]; i+=1
                need(line[:1] in (b' ',b'+',b'-'), 'hunk operation')
                if line[:1]!=b'+':
                    need(cursor<len(source) and source[cursor]==line[1:], 'hunk context mismatch')
                    cursor+=1; used+=1
                if line[:1]!=b'-':result.append(line[1:]); made+=1
                need(used<=n and made<=k, 'hunk line count')
        need(hunks>0, 'empty patch'); result.extend(source[cursor:]); out[name]=b''.join(result)
    return out
def verify_source(stock, inventory, patch):
    expected=dict(inventory['candidate_sha256'])
    for name,digest in inventory['stock_overrides'].items():
        if digest is None: expected.pop(name)
        else: expected[name]=digest
    need(len(expected)==193, 'stock count')
    check_inventory(stock,expected)
    candidate=apply_patch(stock,patch)
    check_inventory(candidate,inventory['candidate_sha256'])
    return len(candidate)
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--stock-src',type=Path,help='local shipping src tree; read-only in-memory reconstruction')
    args=parser.parse_args()
    try:
        files=read_tree(ROOT); docs=check_bundle(files)
        if args.stock_src:
            count=verify_source(read_tree(args.stock_src),docs['source-inventory.json'],files['candidate.patch'])
            print('PASS: 193 authenticated stock files -> %d exact candidate files; no writes or DSP.'%count)
        else:
            print('PASS: package/receipt checks only; source reconstruction and DSP not executed.')
    except (ValueError,KeyError,TypeError,OSError,json.JSONDecodeError) as exc:
        print('FAIL:',exc,file=sys.stderr); return 1
    return 0
if __name__=='__main__':
    sys.exit(main())
