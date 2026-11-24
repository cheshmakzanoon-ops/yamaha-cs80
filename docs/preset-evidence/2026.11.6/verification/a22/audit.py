#!/usr/bin/env python3
"""Independent Bells identity/coverage and extended-register capture receipt."""
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[5]
OUT = Path(__file__).resolve().parent
OLD = ROOT / 'docs/preset-evidence/2026.11.5/baseline/inventory.json'
NEW = ROOT / 'docs/preset-evidence/2026.11.6/verification/a15/inventory.json'
CAPTURE = ROOT / 'build-preset/cs80_preset_capture'
def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    old, new = json.loads(OLD.read_text()), json.loads(NEW.read_text())
    rows = []
    selected = [p for p in new['programs'] if p['category'] == 'Bells']
    old_by_host = {p['host_index']: p for p in old['programs']}
    for p in selected:
        q = old_by_host[p['host_index']]
        assert all(p[k] == q[k] for k in ('index','host_index','name','category'))
        changes = [name for i,name in enumerate(old['parameter_ids'])
                   if name != 'volume' and p['values'][i] != q['values'][i]]
        assert changes, p['name']
        values = dict(zip(new['parameter_ids'], p['values']))
        rows.append({'name':p['name'],'host_index':p['host_index'],
                     'prior_non_volume_changes':changes,
                     'models':[values[f'body{r}_model'] for r in (1,2)],
                     'variants':[values[f'body{r}_variant'] for r in (1,2)],
                     'materials':[values[f'body{r}_material'] for r in (1,2)]})
    assert len(rows) == 23
    updated = {'Tubular Bells','Gong','Temple Bell','Agogo','Music Box'}
    retained = [p['name'] for p in selected if p['name'] not in updated]
    assert len(retained) == 18
    recipe_paths = ['src/core/InstrumentBody.h','src/core/curated/Bells.inc',
                    'src/core/CuratedLibrary.inc','src/core/FactoryPresets.cpp',
                    'tests/MalletPresetRefinementTests.cpp']
    hashes = {p:sha(ROOT/p) for p in recipe_paths}
    exe_before = sha(CAPTURE)
    for velocity, label in ((.25,'soft'),(.85,'firm')):
        cmd=[str(CAPTURE),'--out',str(OUT/label),'--note','84','--velocity',str(velocity),
             '--pressure','0','--hold','.55','--tail','.15']
        for name in retained: cmd += ['--preset',name]
        result=subprocess.run(cmd,cwd=ROOT,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
        (OUT/f'{label}-capture.log').write_text(result.stdout)
        assert result.returncode == 0, result.stdout[-2000:]
    assert sha(CAPTURE)==exe_before
    assert hashes=={p:sha(ROOT/p) for p in recipe_paths}
    report={'source_commit':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'baseline_sha256':sha(OLD),'candidate_sha256':sha(NEW),
            'program_count':len(rows),'identities_preserved':True,
            'all_prior_non_volume_changed':True,'programs':rows,
            'extended_register_capture':{'count':36,'note':84,'velocities':[.25,.85],
                'hold':.55,'tail':.15,'programs':retained,'capture_sha256':exe_before},
            'source_sha256':hashes}
    (OUT/'inventory-and-source.json').write_text(json.dumps(report,indent=2)+'\n')
    print('23 identities preserved; 23 prior non-volume edits; 36 additional captures completed')

if __name__ == '__main__': main()
