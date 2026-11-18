"""Strict integrity-only host validation. No performance gates, clocks or DSP."""
from pathlib import Path
import csv,hashlib,json,io
ROOT=Path(__file__).resolve().parent
DYNAMIC={'mode','authorization','module_path','loaded_module_path','module_device','module_inode','module_size','pid','cpu_before','cpu_after','affinity_before','affinity_after','clock_reads','cpu_ns_sum','wall_ns_sum','window_wall_start_ns','window_wall_end_ns','enclosing_wall_ns'}
def strict(s):
 def pairs(xs):
  d={}
  for k,v in xs:
   if k in d:raise ValueError('duplicate '+k)
   d[k]=v
  return d
 return json.loads(s,object_pairs_hook=pairs,parse_constant=lambda x:(_ for _ in ()).throw(ValueError(x)))
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def need(v,s):
 if not v:raise ValueError(s)
def command(freeze,slot,out,auth):
 return [freeze['roles']['host']['path'],'measure',freeze['roles'][slot['module_role']]['path'],str(out),str(slot['measured_callbacks'])]
def validate(o,slot,freeze,out_dir=None,auth=None,pid=None,before=None,after=None):
 n=slot['measured_callbacks'];ref=strict((ROOT/'expected'/f'{n}.json').read_bytes());role=freeze['roles'][slot['module_role']];module=Path(role['path']).resolve();st=module.stat()
 need(set(o)==set(ref['schema_keys']),'stdout exact schema')
 for k,v in ref['constant_fields'].items():need(type(o[k]) is type(v) and o[k]==v,'constant field '+k)
 need(o['mode']=='measure','measure mode');need(o['authorization']==auth and isinstance(auth,str) and len(auth)==64,'authorization echo')
 need(o['module_path']==str(module) and o['loaded_module_path']==str(module),'actual image path')
 need((o['module_device'],o['module_inode'],o['module_size'])==(st.st_dev,st.st_ino,st.st_size),'actual module identity')
 need(o['pid']==pid and type(pid) is int and pid>0,'actual process identity')
 need(o['cpu_before']==o['cpu_after']==0 and o['affinity_before']==o['affinity_after']=='0','CPU0 affinity')
 need(o['clock_reads']==4*n,'actual clock reads')
 for k in ['clock_reads','cpu_ns_sum','wall_ns_sum','window_wall_start_ns','window_wall_end_ns','enclosing_wall_ns']:need(type(o[k]) is int and o[k]>0,'positive integer clock '+k)
 out=Path(out_dir);expected_files=set(ref['files'])|{'outcome.json','clocks.csv'};need({x.name for x in out.iterdir()}==expected_files,'exact output file set')
 hashes={}
 for name in sorted(expected_files):
  path=out/name;need(path.is_file() and not path.is_symlink(),'regular output '+name);hashes[name]={'sha256':sha(path),'bytes':path.stat().st_size}
  if name in ref['files']:need(hashes[name]==ref['files'][name],'complete artifact '+name)
 need(strict((out/'outcome.json').read_bytes())==o,'stdout/outcome agreement')
 fields=['window_index','global_block','monotonic_start_ns','thread_cpu_start_ns','thread_cpu_end_ns','monotonic_end_ns'];reader=csv.DictReader(io.StringIO((out/'clocks.csv').read_text()));need(reader.fieldnames==fields,'raw clock header');rows=list(reader);need(len(rows)==n,'raw clock count')
 cpu=wall=0;last_cpu=last_wall=0;start=end=None
 for i,row in enumerate(rows):
  need(set(row)==set(fields) and all(isinstance(v,str) and v.isascii() and v.isdigit() and str(int(v))==v for v in row.values()),'canonical integer clocks')
  a={k:int(v) for k,v in row.items()};need(a['window_index']==i and a['global_block']==1024+i,'raw callback order')
  w0,c0,c1,w1=(a[k] for k in fields[2:]);need(0<last_cpu<=c0<c1 if i else 0<c0<c1,'thread clock order');need(0<last_wall<=w0<w1 if i else 0<w0<w1,'wall clock order');need(c1-c0<=w1-w0,'CPU interval enclosed by wall duration')
  cpu+=c1-c0;wall+=w1-w0;last_cpu=c1;last_wall=w1
  if i==0:start=w0
  end=w1
 need(o['cpu_ns_sum']==cpu and o['wall_ns_sum']==wall,'raw clock sums')
 need((o['window_wall_start_ns'],o['window_wall_end_ns'],o['enclosing_wall_ns'])==(start,end,end-start),'enclosing window')
 need(type(before) is int and type(after) is int and before<=start<end<=after,'outer process wall envelope')
 return hashes
