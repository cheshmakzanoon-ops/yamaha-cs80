#!/usr/bin/env python3
"""Small synthetic receipt negatives only; no DSP, compilation, or clock reads."""
import copy
import json
from pathlib import Path
from verify import semantic, read_json, patch_sections, require


def tests():
    here = Path(__file__).resolve().parent
    originals = [read_json(here/n) for n in ('direct-results.json','provenance.json','source-inventory.json')]
    results = []
    semantic(*originals)
    results.append({'test':'saved-field positive control','pass':True})
    negatives = [
        ('wrong accepted count',lambda r,p,i:r['mirror'].__setitem__('accepted',97)),
        ('missing fallback',lambda r,p,i:r['mirror'].__setitem__('transactional_fallbacks',415)),
        ('inflated checks',lambda r,p,i:r['mirror'].__setitem__('checks',27874)),
        ('wrong continuation count',lambda r,p,i:r['mirror'].__setitem__('continuation_steps',12289)),
        ('direct proof mislabeled as audio',lambda r,p,i:r['mirror'].__setitem__('full_engine_render_or_audio_observed',True)),
        ('negative guard accepted',lambda r,p,i:r['guards']['negative_new_TU_flags']['-mfma'].__setitem__('accepted',1)),
        ('fake actual trap',lambda r,p,i:r['traps'].__setitem__('require_actual_SIGFPE',False)),
        ('wrong trap signaled state',lambda r,p,i:r['traps']['observed'][1].__setitem__('mxcsr_hex','1f80')),
        ('V2 execution invented',lambda r,p,i:r['rejected_revision'].__setitem__('runtime_executed',True)),
        ('wrong stock revision',lambda r,p,i:p.__setitem__('stock_commit','0'*40)),
        ('missing source pin',lambda r,p,i:i['stock_sha256'].pop(next(iter(i['stock_sha256'])))),
        ('stock per-TU FP contract removed',lambda r,p,i:p.__setitem__('Ig00156Batch_extra_flags',[])),
        ('unobserved no-PIE recipe',lambda r,p,i:p['historical_direct_link_contract'].__setitem__('explicit_no_pie',True)),
        ('fabricated export runtime',lambda r,p,i:p['export_verification'].__setitem__('new_DSP_executions',1)),
    ]
    for name,mutate in negatives:
        args = copy.deepcopy(originals); mutate(*args)
        try:semantic(*args)
        except (ValueError,KeyError):results.append({'test':name,'pass':True})
        else:raise AssertionError('negative admitted: '+name)
    patch = (here/'candidate-v1.patch').read_text()
    for name,value in [('escaping patch path',patch.replace('a/src/core/Engine.h','a/../Engine.h',1)),
                       ('missing patch section',patch[:patch.index('diff --git a/src/plugin/')])]:
        try:patch_sections(value)
        except ValueError:results.append({'test':name,'pass':True})
        else:raise AssertionError('negative patch admitted: '+name)
    return {'status':'PASS software-only receipt tests','tests':len(results),'negatives':len(results)-1,
            'new_DSP_executions':0,'new_Cpp_builds':0,'records':results}


if __name__ == '__main__':
    print(json.dumps(tests(),indent=2))
