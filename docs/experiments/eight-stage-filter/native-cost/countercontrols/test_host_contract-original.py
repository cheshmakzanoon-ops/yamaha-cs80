#!/usr/bin/env python3
"""Synthetic clock/file-envelope controls from frozen zero-clock gold; no DSP/clocks."""
from pathlib import Path
import json,csv,io,tempfile,shutil,copy,sys,types
import host_contract as hc
ROOT=Path(__file__).resolve().parent

def case(gold,n):
 with tempfile.TemporaryDirectory(prefix='native-contract-synthetic-') as t:
  tmp=Path(t);out=tmp/'out';shutil.copytree(gold,out);module=tmp/'module';module.write_bytes(b'fake never loaded');st=module.stat();auth='1'*64;before=1000000;pid=123
  observed=hc.strict((out/'outcome.json').read_bytes());observed.update(mode='measure',authorization=auth,module_path=str(module),loaded_module_path=str(module),module_device=st.st_dev,module_inode=st.st_ino,module_size=st.st_size,pid=pid,cpu_before=0,cpu_after=0,affinity_before='0',affinity_after='0',clock_reads=n*4)
  rows=[]
  for i in range(n):rows.append([i,1024+i,before+1000+i*5000,500000+i*4000,503000+i*4000,before+5000+i*5000])
  fields=['window_index','global_block','monotonic_start_ns','thread_cpu_start_ns','thread_cpu_end_ns','monotonic_end_ns']
  def clocks(rs):
   s=io.StringIO();w=csv.writer(s,lineterminator='\n');w.writerow(fields);w.writerows(rs);(out/'clocks.csv').write_text(s.getvalue())
  clocks(rows);observed.update(cpu_ns_sum=n*3000,wall_ns_sum=n*4000,window_wall_start_ns=rows[0][2],window_wall_end_ns=rows[-1][-1],enclosing_wall_ns=rows[-1][-1]-rows[0][2]);after=rows[-1][-1]+1000
  freeze={'roles':{'stock':{'path':str(module)}}};slot={'module_role':'stock','measured_callbacks':n}
  def check(o):
   (out/'outcome.json').write_text(json.dumps(o));return hc.validate(o,slot,freeze,out_dir=out,auth=auth,pid=pid,before=before,after=after)
  check(observed);tests=['valid_synthetic_'+str(n)]
  for name,change in [('wrong_module',lambda o:o.__setitem__('loaded_module_path','wrong')),('wrong_authorization',lambda o:o.__setitem__('authorization','2'*64)),('fake_extra_count',lambda o:o.__setitem__('window_calls',512 if n==544 else 544)),('wrong_cpu_sum',lambda o:o.__setitem__('cpu_ns_sum',o['cpu_ns_sum']+1)),('wrong_affinity',lambda o:o.__setitem__('affinity_after','1')),('wrong_endpoint_fp',lambda o:o['endpoint_fp'].__setitem__(0,0))]:
   bad=copy.deepcopy(observed);change(bad)
   try:check(bad)
   except ValueError:tests.append(name)
   else:raise AssertionError('accepted '+name)
  saved=(out/'pcm.bin').read_bytes();corrupt=bytearray(saved);corrupt[-1]^=1;(out/'pcm.bin').write_bytes(corrupt)
  try:check(observed)
  except ValueError:tests.append('wrong_pcm_byte')
  else:raise AssertionError('PCM mutation accepted')
  (out/'pcm.bin').write_bytes(saved)
  clocks(rows[:-1])
  try:check(observed)
  except ValueError:tests.append('missing_clock_row')
  else:raise AssertionError('missing clock accepted')
  return tests
if __name__=='__main__':
 assert len(sys.argv)==3
 print(json.dumps({'passed':True,'real_clock_reads':0,'new_dsp_runs':0,'tests':case(Path(sys.argv[1]),512)+case(Path(sys.argv[2]),544)},indent=2))
