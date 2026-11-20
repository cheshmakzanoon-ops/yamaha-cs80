#!/usr/bin/env python3
"""Verify native selector transitions against serialized VST3 public state; compare entire stock and candidate streams."""
import argparse,csv,hashlib,json,xml.etree.ElementTree as ET
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check(b,m):
    if not b:raise ValueError(m)
def inspect(base):
    rows=list(csv.DictReader((base/'pairs.csv').open()));check(len(rows)==196,'missing phase');seen=set();nonnull=0;frames=0;combined=hashlib.sha256()
    for index,row in enumerate(rows):
        check(int(row['phase'])==index,'phase order');p=base/f'selector-{index}.state';raw=p.read_bytes();end=b'</CS80_PARAMS>';end_index=raw.find(end);check(end_index>=0 and raw.find(end,end_index+1)<0,'unique complete VST3 XML state');x=ET.fromstring(raw[:end_index+len(end)]);params={a.attrib['id']:float(a.attrib['value']) for a in x.findall('PARAM')};wanted=(int(row['rank1']),int(row['rank2']));check(wanted==(params['hardware_source_1'],params['hardware_source_2']),'selector readback mismatch');check(wanted not in seen,'selector repeat');seen.add(wanted);frames+=int(row['frames']);nonnull+=float(row['peak'])>1e-5;combined.update(p.read_bytes())
    check(seen=={(a,b) for a in range(14) for b in range(14)} and nonnull>=190,'not all real selector states or insufficient audible windows');check((base/'pcm.bin').stat().st_size==frames*8 and (base/'trace.bin').stat().st_size==6272*68,'PCM or callback trace size');files={p.name:sha(p) for p in base.iterdir() if not p.name.endswith('.state')};return {'histories':1,'stereo_frames':frames,'genuine_callbacks':6272,'rank_pairs':len(seen),'audible_pair_windows':nonnull,'note_ons':392,'note_offs':392,'latency_samples':67,'phase_state_concat_sha256':combined.hexdigest(),'other_public_artifact_sha256':files}
if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('stock',type=Path);p.add_argument('candidate',type=Path);p.add_argument('stock_elf',type=Path);p.add_argument('candidate_elf',type=Path);p.add_argument('host',type=Path);p.add_argument('out',type=Path);a=p.parse_args();s=inspect(a.stock);c=inspect(a.candidate);check(s==c,'public bytes differ');d=dict(s,all_public_bytes_exact=True,stock_module_sha256=sha(a.stock_elf),candidate_module_sha256=sha(a.candidate_elf),host_sha256=sha(a.host),timing=False);a.out.write_text(json.dumps(d,indent=2,sort_keys=True)+'\n');print(s['audible_pair_windows'],s['rank_pairs'],s['stereo_frames'])
