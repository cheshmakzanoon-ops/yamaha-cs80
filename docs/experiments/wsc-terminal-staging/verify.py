#!/usr/bin/env python3
"""Small receipt replay only: no subprocesses, DSP, benchmarks or network."""
from pathlib import Path
import argparse, hashlib, json, re

BASELINE = 'e9e8175f4278d2457279fcb8809cbba1ddb7be2b'
S_FREEZE = '450a6b6aeff3b105534070e856b2a5659ddd69d3f4ff4d5356cdc8ff24c074b2'
V_FREEZE = '31fedb0323a0f99b0fc87adb040650bf879f226664199472e7aedae3778a77dc'
S_PATCH = '6e215743d2cead9474ebc6940af21a7d9533b67012186b39dd256f9a035bcd19'
V_PATCH = '06387e646e7bdc745630b803b9a7bf965bed4b06e1d3f2c85c81f0f6b517dc96'
SCENES = {'factory_held','factory_moving','panel_held','panel_moving','pure_held','legacy_held','factory_quiet','partial_three','release_wake'}
POSITIVE = {'factory_held','factory_moving','panel_held','panel_moving'}
DATA_FILES = ('provenance.json','scalar-evidence.json','component-evidence.json','integrated-public-probes.json','integrator-hook.json','integrated-vector-evidence.json')
class Invalid(ValueError): pass

def need(ok, why):
    if not ok: raise Invalid(why)

def sha(data): return hashlib.sha256(data).hexdigest()
def digest(x): return isinstance(x,str) and re.fullmatch(r'[0-9a-f]{64}',x) is not None

def load(root):
    return {name:json.loads((root/name).read_text()) for name in DATA_FILES}

def parity(rows):
    need(bool(rows),'empty comparison family')
    for x in rows:
        need(x['equal'] is True,'comparison failed')
        need(x['stock_bytes']==x['candidate_bytes']>0,'comparison byte count differs')
        need(digest(x['stock_sha256']) and x['stock_sha256']==x['candidate_sha256'],'comparison hash differs')

def validate(data):
    p,s,c,v,h=(data[x] for x in DATA_FILES[:5]); g=data['integrated-vector-evidence.json']
    need(p['schema_version']==1 and p['research_only'] is True and p['publication_ready'] is True,'not the research schema')
    need(p['baseline_commit']==BASELINE,'wrong baseline')
    need(set(p['arms'])=={'B','S','V'},'wrong B/S/V labels')
    need(p['source_freezes']=={'S':S_FREEZE,'V':V_FREEZE},'source revision changed')
    need(p['patches']['stock_to_S']['sha256']==S_PATCH and p['patches']['S_to_V']['sha256']==V_PATCH,'patch identity changed')
    need(len(set(p['normal_archives'].values()))==3 and all(digest(x) for x in p['normal_archives'].values()),'archive bindings invalid')
    need(p['normal_archives']=={'B':'323db1f6f973508b9192f310f4bef66efd45e6c6178afc164dd5819b72b5e314','S':'95047fd858b3145ec8655ee4c4fe930faab4ed8cbfc93d811ba8cc31ac7a8094','V':'82ba3c4c0ecd0bb52d67cec8b05b8da332e40dc8e3166a56392a785e4db237ae'},'normal arm identity changed')
    for arm in ('B','S','V'):
        for name,x in p['source_fingerprints'][arm].items():
            need(name.startswith('src/core/') and '..' not in Path(name).parts and digest(x),'invalid source fingerprint')
    need(p['source_fingerprints']['V']['src/core/WscNodeKernelAvx2.cpp']==p['kernel_source_sha256'],'kernel source disconnected')
    need(p['vector_trace']['parent_source_freeze_sha256']==V_FREEZE,'vector observer revision mismatch')
    need(p['vector_trace']['status']=='PASS bounded integrated nine-scene and supplemental qualification','vector trace status mismatch')
    need(not p['strict_build']['global_fast_math'] and not p['strict_build']['global_fma'] and not p['strict_build']['global_native'],'unsafe build scope')
    need(p['strict_build']['translation_unit_extra_flags']=={'src/core/Ig00156Batch.cpp':['-ffp-contract=off'],'src/core/WscNodeKernelAvx2.cpp':['-ffp-contract=off']},'TU contraction contract differs')
    need(s['scope'].startswith('S only; NOT integrated V1b'),'scalar scope relabeled')
    need(s['timing'] is False and s['source_freeze_sha256']==S_FREEZE,'scalar revision/timing changed')
    need(s['trace_driver_sha256']==p['scalar_trace']['driver_sha256'],'scalar driver mismatch')
    need(len(s['scenes'])==9 and {x['scene'] for x in s['scenes']}==SCENES,'missing/duplicate scalar scene')
    comparisons=public_bytes=rng_bytes=draws=callbacks=0
    for x in s['scenes']:
        need(x['callbacks']==1537,'wrong callback boundary count')
        callbacks+=x['callbacks'];draws+=x['rank_rng_draws']
        streams={a['stream']:a for a in x['streams']}
        need(set(streams)=={'.trace.bin','.wsc.bin','.rng.bin'} and len(x['streams'])==3,'missing/duplicate stream')
        for name,a in streams.items():
            need(a['bytes']>0 and digest(a['sha256']),'bad trace fingerprint')
            expected={'rank_draw_association'} if name=='.rng.bin' else {'candidate','counted','rng-stock','candidate-reverse'}
            need(set(a['matched_arms'])==expected and len(a['matched_arms'])==len(expected),'wrong comparison arms')
            comparisons+=len(expected)
            if name=='.rng.bin': rng_bytes+=a['bytes']
            else: public_bytes+=a['bytes']
        need(streams['.rng.bin']['bytes']==x['rank_rng_draws']*12+x['callbacks']*8,'raw RNG framing/count mismatch')
        groups={a['window']:a for a in x['groups']};terminal={a['window']:a for a in x['terminal']}
        need(len(x['groups'])==len(x['terminal'])==2 and set(groups)==set(terminal)=={'onset','late'},'window missing/duplicate')
        for window,g in groups.items():
            wanted=(32768 if window=='onset' else 65536) if x['scene'] in POSITIVE else ((24576 if window=='onset' else 2048) if x['scene']=='release_wake' else 0)
            need(g['samples']==wanted and g['groups']==4*wanted and g['numbered']==[wanted]*4 and g['bad']==0,'group/ordinal invariant')
            t=terminal[window]
            need(t['bad']==t['completed']==t['fallback']==0,'unexpected scalar corpus fallback')
            need(t['deferred']==4*g['groups'] and len(t['masks'])==16 and t['masks']==[0]*15+[g['groups']],'terminal mask/slot invariant')
    need((comparisons,public_bytes,draws,rng_bytes)==(81,253646285,21661858,260052960),'scalar aggregate changed')
    need(s['totals']=={'comparisons':comparisons,'public_private_fp_bytes_per_arm':public_bytes,'ordered_rank_rng_draws':draws,'raw_rank_association_bytes':rng_bytes},'scalar summary disagrees')
    parity(s['reverse_idle_bbd']['comparisons']);parity(s['mixed_engine']['comparisons'])
    need(s['reverse_idle_bbd']['cases']==16 and s['reverse_idle_bbd']['idle_callbacks']==768 and s['reverse_idle_bbd']['bbd_callbacks']==1032,'reverse-control scope changed')
    need(all(x['stock_bytes']==64267367 for x in s['reverse_idle_bbd']['comparisons']),'reverse bytes changed')
    need(s['mixed_engine']['cases']==3 and s['mixed_engine']['masks']==['0xf','0xe','0x3'] and s['mixed_engine']['deferred_lanes']==[32,31,28] and s['mixed_engine']['same_position_original_fallback_lanes']==[0,0,4],'mixed witnesses changed')
    need(all(x['stock_bytes']==461776 for x in s['mixed_engine']['comparisons']),'mixed bytes changed')
    need(c['scope'].startswith('Numerical/component evidence; NOT full integrated'),'component scope relabeled')
    k=c['kernel_adapter'];m=k['component_matrix'];f=c['vector_finish'];z=f['vector_matrix']
    need(m['cohorts']==2354 and m['node_fields']==2354*4*20 and m['transactional_rejections']==30 and m['timing'] is False,'numerical component counts')
    need(k['component_setup']['cases']==192 and k['component_setup']['control_pairs']==24,'setup cases changed')
    need(k['five_mutants_caught'] is True and len(c['kernel_negatives'])==5 and all(x['caught'] for x in c['kernel_negatives']),'negative controls missing')
    forged=[x for x in c['kernel_negatives'] if x['mutant']=='WSC_MUTANT_SCALAR']
    need(len(forged)==1 and forged[0]['exit_code']==0 and forged[0]['structural_packed_work_check'] is False,'scalar impostor structural negative lost')
    need(k['kernel_identity']['packed_instruction_counts']['vdivpd']>0 and k['kernel_identity']['kernel_original_oscillator_method_symbols_absent'] is True,'no actual packed work/isolation')
    need(z['kernel_attempts']==9414 and z['kernel_successes']==9411 and z['kernel_rejections']==3 and z['kernel_attempts']==z['kernel_successes']+z['kernel_rejections'],'component attempt/success/rejection mismatch')
    need(z['checks']==42772633 and f['scalar_binding_matrix']['kernel_attempts']==0,'component positive/control counts')
    need(f['engine_edits'] is False and f['timing'] is False and f['published'] is False,'component scope expanded')
    need(f['actual_sigfpe']['seed_pairs']==5 and f['actual_sigfpe']['ab_signals']==f['actual_sigfpe']['aa_signals']==10,'component trap count')
    need(f['record_consumption_negative']['expected_exit']==f['record_consumption_negative']['actual_exit']==1 and f['record_consumption_negative']['caught']=='raw diagnostics','consumed-record negative lost')
    need(v['scope'].startswith('Actual public Engine::render, integrated V1b') and v['no_vector_success_census_in_this_fixture'] is True,'public probe scope changed')
    need(v['cases']==22 and v['expected_real_sigfpe']==7 and v['continuation_public_frames']==4 and v['frames_per_call']==1,'public probe cases changed')
    need(v['source_sha256']==p['public_probe_sources']['render-traps.cpp'] and v['observer_sha256']==p['public_probe_sources']['observe_engine.h'],'public probe source disconnected')
    need(len(v['arms'])==2 and {a['arm']for a in v['arms']}=={'normal','reverse'},'public probe arm missing')
    for a in v['arms']:
        need(a['all_state_fp_pcm_sample_bytes_equal'] is True and a['sig_status_sites_equal'] is True and a['disagreeing_cases']==[],'public probe mismatch')
        need(a['genuine_in_renderer_sigfpe']==7 and a['bytes_per_arm']==17019552 and a['sha256']=='2eb309790f0460b355a753fe9c3cd30ef91eae7572078a362419da743d7d96ec' and digest(a['executable_sha256']),'public probe result changed')
    st=v['static'];critical=list(st['critical_functions'].values())
    need(len(critical)==3 and sorted(x['stock_bytes']for x in critical)==[12063,32154,37511],'critical function roster')
    need(all(a['bytes_equal'] is True and a['symbolic_relocations_equal'] is True and a['stock_bytes']==a['candidate_bytes']for a in critical),'critical identity failed')
    need(st['two_exact_patch_reconstructions'] is True and st['no_extra_critical_definitions'] is True and st['kernel_whole_object_equal'] is True,'static isolation/reconstruction failed')
    g=data['integrated-vector-evidence.json']
    need(g['scope'].startswith('Integrated V1b') and g['status']=='PASS bounded integrated qualification' and g['timing'] is False,'integrated scope/status')
    need(g['source_freeze_sha256']==V_FREEZE and g['trace_driver_sha256']==p['vector_trace']['driver_sha256'],'integrated source/driver mismatch')
    need(len(g['scenes'])==9 and {r['scene']for r in g['scenes']}==SCENES,'integrated scene missing/duplicate')
    ncomp=nbytes=ndraw=nrng=0
    for r in g['scenes']:
        need(r['callbacks']==1537,'integrated callback count')
        ndraw+=r['rank_rng_draws'];streams={x['stream']:x for x in r['streams']}
        need(len(r['streams'])==3 and set(streams)=={'.trace.bin','.wsc.bin','.rng.bin'},'integrated stream roster')
        for name,x in streams.items():
            expect={'rank_draw_association'}if name=='.rng.bin'else{'vector','counted','rng-stock','reverse'}
            need(set(x['matched_arms'])==expect and len(x['matched_arms'])==len(expect) and digest(x['sha256']),'integrated comparison arms/hash')
            ncomp+=len(expect)
            if name=='.rng.bin':nrng+=x['bytes']
            else:nbytes+=x['bytes']
        need(streams['.rng.bin']['bytes']==r['rank_rng_draws']*12+r['callbacks']*8,'integrated RNG framing')
        need(len(r['windows'])==2 and {w['window']for w in r['windows']}=={'onset','late'},'integrated windows')
        for w in r['windows']:
            window=w['window'];wanted=(32768 if window=='onset'else 65536)if r['scene']in POSITIVE else((24576 if window=='onset'else 2048)if r['scene']=='release_wake'else 0)
            st=w['stage'];bg=w['by_group'];sp=w['span']
            need(st['samples']==wanted and st['groups']==4*wanted and st['numbered']==[wanted]*4 and st['bad']==0,'integrated numbered stage counts')
            for key in ('attempts','successes','owner_checks'):need(bg[key]==[wanted]*4,'integrated actual success/owner counts')
            need(bg['known']==[4*wanted]*4,'successful vector records not consumed')
            for key in ('rejections','scalar','bad'):need(bg[key]==[0]*4,'unexpected integrated corpus rejection/scalar/bad')
            need(w['full_pending_mask_groups']==[wanted]*4 and w['other_pending_mask_groups']==[0]*4 and w['both_bindings_present']==[wanted]*4 and w['missing_bindings']==[0]*4,'pending masks or owner bindings')
            renders=256 if window=='onset'else 512;selected=wanted//128
            need(sp['renders']==renders and sp['frames']==64*renders and sp['selections']==sp['selected']==selected and sp['bad']==0,'render-span setup count')
            need(sp['preparations']==sp['valid']==[selected]*4,'render-span preparation lifetime count')
    need((ncomp,nbytes,ndraw,nrng)==(81,253646285,21661858,260052960),'integrated aggregate changed')
    need(g['totals']=={'comparisons':ncomp,'public_private_fp_bytes_per_arm':nbytes,'raw_rank_association_bytes':nrng,'ordered_rank_rng_draws':ndraw},'integrated summary differs')
    need(set(g['supplements'])=={'semantic','blocks','reverse_idle_bbd'},'missing integrated supplement')
    for key,size,arms in [('semantic',923244,3),('blocks',124904389,4),('reverse_idle_bbd',64267367,2)]:
        rr=g['supplements'][key]['comparisons'];parity(rr)
        need(len(rr)==arms and all(x['stock_bytes']==size for x in rr),'supplement size/arm mismatch')
    need(g['supplements']['semantic']['cases']==6,'semantic cases')
    br=g['supplements']['blocks'];need(br['rng']['equal'] is True and br['rng']['bytes']==4619136 and digest(br['rng']['sha256']),'block RNG mismatch')
    need(set(br['partition_checkpoint_tally'])=={'stock','vector','counted','rng-stock','reverse'} and all(x=={'equal_to_one_frame':4,'different_from_one_frame':16}for x in br['partition_checkpoint_tally'].values()),'partition scope changed')
    need(h['schema_version']==2 and h['cut_id']==p['cut_id'] and h['required_source_freezes']==p['source_freezes'],'integration parent mismatch')
    ih=h['integrated_V1b_full_trace'];need(ih['status']=='PASS_BOUNDED' and ih['receipt']=='integrated-vector-evidence.json' and ih['comparisons']==81 and ih['source_freeze_sha256']==V_FREEZE,'integrated completion binding')
    ch=h['cpu_timing'];need(ch['status']=='COMPLETE_NO_ADMISSION' and ch['receipt']=='cost/result.json' and ch['readme']=='cost/README.md','cost hook binding')
    need(ch['slots']==216 and ch['complete'] is True and ch['threshold_gate_passes'] is False and ch['real_core_threshold_admission'] is False,'cost completion/admission')
    need(ch['decision']=='inconclusive / no performance admission in this environment','unsupported performance decision')
    need(ch['analysis_sha256']==p['receipt_fingerprints']['cost/actual-analysis'],'cost analysis disconnected')
    need(ch['predeclared_rule_result_floor']==0.21484557952526984,'cost floor changed')
    need(ch['descriptive_scalar_saving_percent']=={'factory-held':9.453532744613637,'factory-moving':6.6740134832387055},'descriptive S/B contrast changed')
    need(ch['primary_V_vs_B_ci95_percent'][0]<100*ch['predeclared_rule_result_floor'],'unsupported primary admission')
    need(h['production_promotion'] is False and h['native_plugin_qualification'] is False,'unsupported production/native claim')
    return {'status':'PASS receipt replay only','scalar_comparisons':comparisons,'integrated_comparisons':ncomp,'public_probe_cases':22,'component_vector_successes':9411,'cpu_timing':'COMPLETE_NO_ADMISSION'}

def verify_package(root):
    manifest=json.loads((root/'manifest.json').read_text())
    need(manifest['research_only'] is True and manifest['status']=='sealed-research-evidence','manifest not sealed research')
    expected=set(manifest['files'])|{'manifest.json'}
    actual={p.name for p in root.iterdir() if p.is_file()}
    need(all(p.name in ('cost','__pycache__')for p in root.iterdir()if p.is_dir()),'unexpected subtree')
    need(actual==expected,'inventory changed')
    for name,entry in manifest['files'].items():
        need(not Path(name).is_absolute() and '..' not in Path(name).parts,'unsafe manifest path')
        raw=(root/name).read_bytes()
        need(len(raw)==entry['bytes'] and sha(raw)==entry['sha256'],'file checksum: '+name)
        text=raw.decode('utf-8')
        need(all(x not in text for x in ('/'+'workspace'+'/','/'+'home'+'/agent/','sandbox'+':')),'private machine path')
    data=load(root)
    for item in ('stock_to_S','S_to_V'):
        p=data['provenance.json']['patches'][item]
        need(sha((root/p['file']).read_bytes())==p['sha256'],'canonical patch checksum')
    for name,h in data['provenance.json']['public_probe_sources'].items():
        need(sha((root/name).read_bytes())==h,'portable probe changed')
    return validate(data)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--package',type=Path,default=Path(__file__).resolve().parent)
    args=parser.parse_args()
    try:print(json.dumps(verify_package(args.package),indent=2))
    except (Invalid,KeyError,ValueError,TypeError,OSError) as e:parser.exit(1,'FAIL: '+str(e)+'\n')
