"""Small adversarial replay tests; never runs the sampler or loads a plugin."""
import copy, json, shutil, sys, tempfile
from pathlib import Path
sys.dont_write_bytecode=True
import verify

base=verify.load();verify.replay(base);results=[]
def rejected(name, mutation):
    trial=copy.deepcopy(base)
    mutation(trial)
    try:
        verify.replay(trial)
    except (ValueError,AssertionError,KeyError,TypeError,IndexError) as error:
        results.append({'test':name,'rejected':True,'reason':str(error)})
    else:
        raise AssertionError('tamper was accepted: '+name)

rejected('missing module SHA',lambda b:b['modules.json']['modules']['main'].pop('sha256'))
rejected('changed pinned module SHA',lambda b:b['modules.json']['modules']['main'].__setitem__('sha256','0'*64))
rejected('missing module',lambda b:b['modules.json']['modules'].pop('libm'))
rejected('unknown PC module',lambda b:b['pc-counts.json']['pcs'][0].__setitem__(0,'other'))
rejected('ASLR-like PC outside executable ranges',lambda b:b['pc-counts.json']['pcs'][0].__setitem__(1,'0x7fff12345678'))
main_index=next(i for i,r in enumerate(base['pc-counts.json']['pcs']) if r[0]=='main')
lib_index=next(i for i,r in enumerate(base['pc-counts.json']['pcs']) if r[0]=='libm')
rejected('missing DWARF annotation',lambda b:b['pc-counts.json']['pcs'][main_index].__setitem__(3,None))
rejected('invalid frame reference',lambda b:b['symbols.json']['chains'][0].__setitem__(0,10**9))
rejected('duplicate normalized PC',lambda b:b['pc-counts.json']['pcs'].append(copy.deepcopy(b['pc-counts.json']['pcs'][0])))
rejected('dropped sample input',lambda b:b['pc-counts.json']['captures'][0]['counts'].pop())
rejected('duplicate counted PC',lambda b:b['pc-counts.json']['captures'][0]['counts'].append(copy.deepcopy(b['pc-counts.json']['captures'][0]['counts'][0])))
rejected('zero sample count',lambda b:b['pc-counts.json']['captures'][0]['counts'][0].__setitem__(1,0))
rejected('negative sample count',lambda b:b['pc-counts.json']['captures'][0]['counts'][0].__setitem__(1,-1))
rejected('invented exclusive owner',lambda b:b['captures.json']['captures'][0]['exclusive_owner_counts'].__setitem__('invented caller',1))
rejected('double-charged library caller',lambda b:b['pc-counts.json']['pcs'][lib_index].__setitem__(2,0))
rejected('calibration PC outside known loop',lambda b:b['pc-counts.json']['captures'][0]['calibration_counts'][0].__setitem__(0,'0x0'))
rejected('calibration count changed',lambda b:b['captures.json']['captures'][0].__setitem__('calibration_samples',999))
rejected('changed prefix PCM receipt',lambda b:b['captures.json']['captures'][3].__setitem__('prefix4096_hash','0'*16))
rejected('missing eight-voice check',lambda b:b['captures.json']['captures'][0].__setitem__('voice_checks',0))
rejected('empty capture signal',lambda b:b['captures.json']['captures'][0].__setitem__('nonzero',0))
rejected('changed source route',lambda b:b['captures.json']['captures'][0].__setitem__('routes',[1,1]))
rejected('private absolute source path',lambda b:b['symbols.json']['frames'][0].__setitem__(1,'/private/example/source.cpp:1'))
rejected('changed planned slot order',lambda b:b['captures.json']['captures'][0].__setitem__('period_us',5003))
def unused_pc(b):
    row=copy.deepcopy(b['pc-counts.json']['pcs'][main_index]);used={int(x[1],16) for x in b['pc-counts.json']['pcs'] if x[0]=='main'}
    addr=int(row[1],16)+1
    while addr in used:addr+=1
    row[1]=hex(addr);b['pc-counts.json']['pcs'].append(row)
rejected('unused annotation/PC coverage',unused_pc)

with tempfile.TemporaryDirectory() as temporary:
    root=Path(temporary)
    for name in verify.INPUTS:shutil.copyfile(verify.ROOT/name,root/name)
    target=root/'pc-counts.json';target.write_bytes(target.read_bytes()+b' ')
    try:verify.check_content_anchors(root)
    except ValueError as error:results.append({'test':'byte/content-anchor mutation','rejected':True,'reason':str(error)})
    else:raise AssertionError('content anchor mutation accepted')
print(json.dumps({'positive_replay_passed':True,'negative_tests':len(results),'all_rejected':True,'tests':results},indent=2,sort_keys=True))
