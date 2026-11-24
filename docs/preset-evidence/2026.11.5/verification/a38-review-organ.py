#!/usr/bin/env python3
"""Summarize A38's bounded organ diagnostics without declaring native qualification."""
from __future__ import annotations
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
EVIDENCE = ROOT / 'docs/preset-evidence/2026.11.5'
HERE = Path(__file__).resolve().parent
EXPECTED = [
 'Tone I: Organ 1', 'Tone II: Organ 2', 'Flute Organ 8+4', 'Flute Organ 16+8',
 'Percussive Organ', 'Reed Organ', 'Church Organ', 'Chapel Organ', 'Theatre Organ',
 'Tremolo Organ', 'Mellow Organ', 'Bright Organ', 'Pipe Organ', 'Combo Organ',
 'Fifth Organ', 'Organ Vibrato', 'Tremolo LFO Organ',
]

def read(path):
    return json.loads(path.read_text())

def sha(path):
    b=path.read_bytes()
    return {'path':str(path.relative_to(ROOT)) if path.is_relative_to(ROOT) else str(path),
            'bytes':len(b),'sha256':hashlib.sha256(b).hexdigest()}

def main():
    old = read(HERE/'a38-organ-baseline.json')
    new = read(HERE/'a38-organ-final.json')
    bi = read(EVIDENCE/'baseline/inventory.json')
    ai = read(EVIDENCE/'inventory.json')
    checks=[]
    def check(ok, label):checks.append({'check':label,'passed':bool(ok)})
    check(old['revision']=='2026.11.4' and new['revision']=='2026.11.5','explicit before/after revisions')
    check(old['sample_rate']==new['sample_rate']==48000,'matched 48 kHz output')
    check([p['name'] for p in old['phrases']]==[p['name'] for p in new['phrases']]==EXPECTED,'all 17 existing organ identities')
    by_old={p['name']:p for p in old['phrases']}
    compare=[]
    for p in new['phrases']:
        name=p['name']
        check(p['peak_dbfs']<0,name+': full phrase has headroom')
        check(min(p[k] for k in ['first_held_dbfs','second_held_dbfs','repeated_held_dbfs'])>-70,name+': all three chord segments audible')
        check(p['tail_dbfs']<-60,name+': final tail clears below -60 dBFS')
        check(p['tail_relative_db']<-40,name+': final tail falls over 40 dB below held chord')
        check(p['velocity_max_absolute_difference']==0,name+': swapped velocities leave exact PCM unchanged')
        check(p['pressure_max_absolute_difference']>1e-5 if name=='Tremolo LFO Organ' else p['pressure_max_absolute_difference']==0,
              name+': pressure follows the authored route')
        compare.append({'name':name,'before_tail_relative_db':by_old[name]['tail_relative_db'],
                        'after_tail_relative_db':p['tail_relative_db'],
                        'after_peak_dbfs':p['peak_dbfs'],
                        'velocity_exact_pcm_equal':p['velocity_max_absolute_difference']==0,
                        'unrouted_pressure_exact_pcm_equal':p['pressure_max_absolute_difference']==0})
    check(len(new['registration'])==10,'ten separate register paths inspected')
    for r in new['registration']:
        label=r['name']+f" rank {r['rank']}"
        check(r['expected_line_dbfs']>-70,label+': named fundamental present')
        check(r['fundamental_over_other_tested_footages_db']>3,label+': expected line dominates other tested footage lines')
        check(r['peak_dbfs']<0,label+': isolated rank has headroom')
    modern={r['rank']:r for r in new['modern_pressure']}
    check(set(modern)=={0,1,2},'complete patch and both rank pressure controls')
    check(modern[0]['gain_db']>.1,'modern organ: complete-patch pressure level increases')
    check(modern[0]['octave_to_root_change_db']>.5,'modern organ: pressure brings upper stop forward in full patch')
    check(modern[1]['max_absolute_difference']==0,'modern organ: isolated lower stop is pressure invariant')
    check(modern[2]['gain_db']>2,'modern organ: isolated upper stop responds to pressure')
    for rank,r in modern.items():check(r['peak_dbfs']<0,f'modern pressure rank {rank}: headroom')
    before={p['name']:p for p in bi['programs']}
    after={p['name']:p for p in ai['programs']}
    stable_controls=['ch1_feet','ch2_feet']+[f'body{rank}_{field}' for rank in [1,2]
                         for field in ['model','mix','brightness','decay','size']]
    edits=[]
    for name in EXPECTED:
        b=dict(zip(bi['parameter_ids'],before[name]['values']))
        a=dict(zip(ai['parameter_ids'],after[name]['values']))
        check(all(a[k]==b[k] for k in stable_controls),name+': published stop pitches/body registrations preserved')
        check(all(a[f'ch{r}_init_{k}']==0 for r in [1,2] for k in ['brill','level']),name+': no strike-velocity routes')
        check(all(a[k]==0 for k in ['detune','voice_dispersion','drift']),name+': straight stop tuning')
        check(all(a[f'body{r}_variant']==0 for r in [1,2]),name+': ordinary organ material retained')
        edits.append({'index':after[name]['index'],'name':name,'changed_controls':
                      {k:{'before':b[k],'after':a[k]} for k in b if a[k]!=b[k]}})
    inputs=[sha(p) for p in [HERE/'a38-organ-probe.cpp',HERE/'a38-review-organ.py',
            HERE/'a38-organ-baseline.json',HERE/'a38-organ-final.json',
            EVIDENCE/'baseline/inventory.json',EVIDENCE/'inventory.json',
            ROOT/'build-preset/libcs80_core.a',ROOT.parent/'baseline-build/libcs80_core.a']]
    report={
        'schema':'cs80-a38-organ-review-v1','source_commit':'6094bce18c8b19da804c2fbc9a00e31969a7f579',
        'release_qualified':False,'native_validation_claimed':False,
        'methods':{
            'versions':'Each probe is compiled against its matching source headers and existing full core static archive.',
            'phrase':'2.2 s; four notes [48,55,60,64] at 0 s, four notes [50,57,62,65] at 0.5 s; first off at 0.54 s; second off at 0.98 s; first chord repeated at 1.1 s, released 1.6 s.',
            'velocity':'Reference 0.2/0.9/0.2 versus swapped 0.9/0.2/0.9; compare all float samples.',
            'pressure':'Reference pressure zero versus pressure 1 at 0.25/1.3 s, zero at 0.75/1.5 s; all other events unchanged.',
            'tail':'1.8-2.0 s RMS relative to held 1.3-1.5 s; includes named native chorus/tremolo.',
            'registration':'MIDI60; mix endpoint isolates a rank; 0.5-1.1 s Hann projection at explicit nominal frequencies compared with 0.5,1,1.5,2,4 times the keyboard pitch. This is line-presence evidence, not a complete pitch estimator.',
            'pressure_stop':'MIDI60, velocity0.7, pressure0/1, full mix and each isolated rank; 0.5-1.0 s RMS; projection compares 2f/f changes.',
            'render_count_per_version':67,'frames_per_version':6307200,'seconds_per_version':131.4,
            'limits':'No reference organ recordings or listening study; these checks establish routing, registrations, chord keying and numerical integrity, not acoustic indistinguishability. The diagnostic limits are review sanity bounds, not newly introduced production release gates.',
        },
        'checks':checks,'check_count':len(checks),'failures':[x for x in checks if not x['passed']],
        'phrase_comparison':compare,'final_registration':new['registration'],
        'modern_pressure_before':old['modern_pressure'],'modern_pressure_after':new['modern_pressure'],
        'recipe_changes':edits,'inputs':inputs,
    }
    output=HERE/'a38-organ-review.json'
    output.write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'checks':len(checks),'failures':report['failures'],
          'peak_dbfs_max':max(p['peak_dbfs'] for p in new['phrases']),
          'tail_relative_db_max':max(p['tail_relative_db'] for p in new['phrases']),
          'modern_pressure':new['modern_pressure']},indent=2))
    return bool(report['failures'])

if __name__=='__main__':raise SystemExit(main())
