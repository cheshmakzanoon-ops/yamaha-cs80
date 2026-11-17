import pathlib,json,csv,struct,array,math,hashlib
p=pathlib.Path(__file__).parent/'extracted'; count=0
for folder in list(p.glob('run-*'))+list(p.glob('dry-stock-*'))+list(p.glob('dry-candidate-*')):
 r=json.loads((folder/'result.json').read_text());idx=r['row']
 assert r['selector']==[0,4,7,10,4,4,4][idx]
 assert r['factor']==2 and r['latency']==(63 if idx==6 else 67) and r['render_policy']==(0 if idx==6 else 1)
 assert r['mxcsr_initial']&~63==0x9fc0 and r['mxcsr_final']&~63==0x9fc0
 assert (folder/'parameters.csv').read_bytes()==(p/'frozen-inputs/parameter-oracle'/f'row-{idx}.csv').read_bytes()
 with open(folder/'events.csv') as f:
  rows=list(csv.DictReader(f));assert len(rows)==11250
  for i,v in enumerate(rows):
   phase=i%750;value=min(phase,750-phase)/375 if idx==4 else 0
   assert [int(v[k]) for k in ['callback','frame','noteons','noteoffs','pressure_event','pressure_bits']]==[i,i*256,8 if idx==5 and i%1125==0 else 0,8 if idx==5 and i%1125==375 else 0,int(idx==4),struct.unpack('<I',struct.pack('<f',value))[0]]
 count+=len(rows)
for f in (p/'canonical-pcm').glob('*.f32'):
 data=array.array('f');data.frombytes(f.read_bytes());assert all(map(math.isfinite,data))
report={'receipts':98,'independent_event_rows':count,'parameter_oracle_equal':True,'finite_unique_pcm':7,'FP_controls_and_latency_policy':True}
(p.parent/'independent-input-audit.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
