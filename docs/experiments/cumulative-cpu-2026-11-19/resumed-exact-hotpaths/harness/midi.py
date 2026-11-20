#!/usr/bin/env python3
"""Run connected public MIDI mappings; verify each control audibly changes the shared scene."""
from pathlib import Path
import hashlib,json,os,struct,subprocess,argparse
MASKS=(0,7,11,13,14,15);NAMES={7:'modulation_wheel',11:'channel_pressure',13:'member_pitch_bend',14:'master_pitch_bend'}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check(b,s):
    if not b:raise ValueError(s)
def receive(folder):
    out={};full=(folder/'15/pcm.bin').read_bytes();check(len(full)==512*64*2*4,'length');influence={}
    for k in MASKS:
        d=folder/str(k);fps={p.name:sha(p) for p in sorted(d.iterdir())};check(set(fps)=={'delivered.csv','pcm.bin','fp.bin','initial-metadata.csv','final-metadata.csv','initial.state','outcome.json'}|{f'after-{b}.state' for b in (127,226,355,511)},'files');j=json.loads((d/'outcome.json').read_text());check(j['actual_callbacks']==512 and j['latency_samples']==67 and j['mask']==k and j['controller_points_sent']>120 and j['note_ons']==j['note_offs']==16,'workload');out[str(k)]={'sha256':fps,'outcome':j};
        if k in NAMES:
            data=(d/'pcm.bin').read_bytes();check(len(data)==len(full),'short capture');aa=struct.iter_unpack('<I',full);bb=struct.iter_unpack('<I',data);changes=[i for i,(x,y) in enumerate(zip(aa,bb)) if x[0]!=y[0]];check(len(changes)>0,'one controller had no audible PCM effect: '+NAMES[k]);influence[NAMES[k]]={'different_f32_words':len(changes),'first_different_stereo_frame':changes[0]//2}
    return {'cells':out,'leave_one_control_neutral_vs_full':influence}
def take(a):
    root=a.out.resolve();root.mkdir();elf=a.module.resolve(strict=True);host=a.host.resolve(strict=True);mh=sha(elf);hh=sha(host)
    home=root/'home';home.mkdir();env=dict(os.environ,LC_ALL='C',TZ='UTC',HOME=str(home),XDG_CONFIG_HOME=str(home/'config'),XDG_CACHE_HOME=str(home/'cache'))
    for m in MASKS:subprocess.run([str(host),str(elf),str(m),str(root/str(m))],env=env,timeout=60,check=True)
    data=receive(root);check(sha(elf)==mh and sha(host)==hh,'input changed');(root/'identities.json').write_text(json.dumps({'host_sha256':hh,'module_sha256':mh})+'\n');return data
def compare(a):
    stock=receive(a.stock);cand=receive(a.candidate);check(stock==cand,'mismatch in native public PCM/FP/latency/metadata/state/controls');sa=json.loads((a.stock/'identities.json').read_text());ca=json.loads((a.candidate/'identities.json').read_text());check(sa['host_sha256']==ca['host_sha256'],'same host');data={'stock_module_sha256':sa['module_sha256'],'candidate_module_sha256':ca['module_sha256'],'host_sha256':sa['host_sha256'],'closures_exact':True,'native_histories':len(MASKS),'callbacks_per_arm':len(MASKS)*512,'stereo_frames_per_arm':len(MASKS)*32768,'timing':False,**stock};a.out.write_text(json.dumps(data,indent=2,sort_keys=True)+'\n');print(json.dumps(data['leave_one_control_neutral_vs_full']))
if __name__=='__main__':
    ap=argparse.ArgumentParser(description=__doc__);ps=ap.add_subparsers(dest='op',required=True);ss=ps.add_parser('take');ss.add_argument('module',type=Path);ss.add_argument('out',type=Path);ss.add_argument('--host',type=Path,required=True);ss=ps.add_parser('compare');ss.add_argument('stock',type=Path);ss.add_argument('candidate',type=Path);ss.add_argument('out',type=Path);a=ap.parse_args();take(a) if a.op=='take' else compare(a)
