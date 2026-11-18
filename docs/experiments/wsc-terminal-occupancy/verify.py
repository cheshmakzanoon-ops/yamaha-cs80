#!/usr/bin/env python3
"""Validate the published receipts and descriptive stock-only counts. Does not run DSP."""
from pathlib import Path
import collections, csv, hashlib, json
P=Path(__file__).resolve().parent
J=lambda p:json.loads(p.read_text()); H=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
def check(ok,msg):
    if not ok: raise ValueError(msg)
def group(row,s):
    mod,_,fn,ch=row
    if mod!='main':return 'outside_candidate'
    frames=[(s['functions'][s['frames'][i][0]],s['frames'][i][1]) for i in s['chains'][ch]]
    key='::ReconstructedOscillator::nodes('
    if key in s['functions'][fn] or any(key in n for n,loc in frames):return 'all_nodes_including_inlined_descendants'
    for n,loc in frames:
        if '::ReconstructedOscillator::tick(' in n and loc.startswith('project/core/ReconstructedOscillator.h:'):
            line=int(loc.split(':')[1].split(' ')[0]);
            if 313<=line<=318:return 'tick_output_epilogue_lines313to318'
            if line==309:return 'line309_endpoint_materialize'
    return 'outside_candidate'
def run(root=P,profile=None):
    e=J(root/'evidence.json');f=root/'observed-counts.csv';check(H(f)==e['observed_counts_sha256'],'raw counter hash')
    parts=e['stream_receipts'];main=[r for r in parts if r['scene']!='reporting-only-perturb'];check(len(parts)==20 and len(main)==18,'stream count')
    check(set((r['scene'],r['stream']) for r in main)=={(s,t) for s in e['scenes'] for t in ['.wsc.bin','.trace.bin']},'stream key matrix')
    check(all(r['equal'] and len(bytes.fromhex(r['reference_sha256']))==32 and r['reference_sha256']==r['observed_sha256'] for r in parts),'paired trace hash receipt')
    check(sum(r['bytes'] for r in main)==e['raw_primary_stream_bytes_per_arm']==46_558_089,'trace bytes')
    with f.open() as file:rows=list(csv.DictReader(file))
    keys=set();window=collections.defaultdict(collections.Counter)
    for rr in rows:
        r=dict(rr);sc,w,g=r.pop('scene'),r.pop('window'),int(r.pop('group'));k=(sc,w,g)
        check(k not in keys and sc in e['scenes'] and w in ('late','onset') and 0<=g<4,'group key');keys.add(k);r={k:int(x) for k,x in r.items()};check(all(v>=0 for v in r.values()),'negative count')
        check(r['samples']==(65536 if w=='late' else 32768),'group opportunities');check(r['samples']==sum(r['active'+str(n)] for n in range(4))+r['complete4'],'partition')
        check(r['duplicates']==0,'duplicate lanes') if 'duplicates' in r else check(r['duplicate_ticks']==0,'duplicate ticks');check(r['completed_ticks']==r['ticks']==sum(n*r['active'+str(n)] for n in (1,2,3))+4*r['complete4'],'completion');check(r['failed_lanes']==0,'unexpected failure')
        check(r['strong_unchanged_stable_packs']<=r['cheap_gate_packs']<=r['complete4'] and r['terminal_ordinary_nodes_packs']<=r['complete4'],'subset counts');window[sc,w].update(r)
    check(len(rows)==len(keys)==72,'full group matrix');held=window['factory_held','late'];moving=window['factory_moving','late'];
    for r,strong,coarse in [(held,218870,775),(moving,29482,423)]:check((r['complete4'],r['terminal_ordinary_nodes_packs'],r['strong_unchanged_stable_packs'],r['diag_zero_but_boundary_packs'])==(262144,262144,strong,coarse),'factory census')
    for sc in ['legacy_held','factory_quiet']:check(window[sc,'late']['ticks']==0,'inactive case')
    z=window['partial_three','late'];check((z['complete4'],z['active2'],z['active0'])==(65536,65536,131072),'partial group case')
    z=e['real_reset_control'];check(z['bytes_per_arm']==7031 and z['reference_observed_equal'] and z['matched_arm_object_link'],'reset receipt');a,b=z['rows'];check((a['ticks'],a['complete4'],a['terminal_packs'],a['terminal_lanes'],a['physical_steps'])==(4,1,1,4,0) and (b['ticks'],b['complete4'],b['terminal_packs'],b['terminal_lanes'],b['physical_steps'],b['reset_end_records'])==(4,1,0,3,1,1),'actual reset-boundary counter')
    z=e['reporting_control'];check(z['detected'] and z['dsp_traces_unchanged'] and z['differences']==[dict(window='late',group='0',field='terminal_ordinary_nodes_packs',normal='65536',perturbed='65537')],'reporting-only mutant')
    prefix=(root/'census.cpp').read_bytes()[:e['semantic_prefix_bytes']];check(hashlib.sha256(prefix).hexdigest()==e['semantic_prefix_sha256'],'original semantic fixture prefix')
    prof=profile or root.parent/'shipping-core-profile';check(all(H(prof/f)==sha for f,sha in e['profile_sources_sha256'].items()),'published profile provenance');s=J(prof/'symbols.json');p=J(prof/'pc-counts.json');caps=J(prof/'captures.json')['captures'];saved=J(root/'profile-scope.json')['rows'];check(len(caps)==len(saved)==len(p['captures'])==6,'profile count')
    for cap,pcap,want in zip(caps,p['captures'],saved):
        counts=collections.Counter()
        for i,n in pcap['counts']:counts[group(p['pcs'][i],s)]+=n
        check((cap['scene'],cap['period_us'],sum(counts.values()))==(want['scene'],want['period_us'],cap['samples']) and dict(counts)==want['counts'],'profile regroup')
        check(want['samples']==cap['samples'] and all(abs(want['pct'][k]-v*100./cap['samples'])<1e-12 for k,v in counts.items()),'profile fraction')
    return '72 fixed-group/window rows, 18 primary paired stream receipts, 2 independent counter controls and 6 profile regroupings verified. No DSP executed.'
if __name__=='__main__':print(run())
