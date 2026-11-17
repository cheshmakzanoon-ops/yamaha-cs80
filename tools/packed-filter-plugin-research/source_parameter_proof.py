"""Independent bounded parent checks for the immutable SourceAwarePanel history."""
import hashlib,math,struct
from failure_metadata_decode import Reader,connected_metadata,classify_roster,require
from source_parameter_roles import GENERATED,MEMORY,expected_metadata
IDS=dict(GENERATED);PANEL=(IDS['hardware_source_1'],IDS['hardware_source_2'])
ALTER=((IDS['volume'],0x3fcc28f5c0000000),(IDS['ch1_lpf_cutoff'],0x3fddd9fa60000000))
ONE=struct.pack('<d',1.0)
def case_controls(data):
 r=Reader(bytes(data))
 def text():
  n=r.u32();require(n<=4096,'case text bound');return r.raw(n).decode('utf8')
 require(text()=='packed-filter-workload-draft-v1','source case schema');require(text()=='eb5b73f144f83ff386865b0abb726f767eb8f4a0b651662442c4d03ccf8a2ac2' and text()=='dcc9bc824420ceb801d5047660ff4857a9ab4adda9b4762d0bd8df09aba08981','source case provenance');fields=[r.u32() for _ in range(6)];require(fields[0] in (0,1,2) and fields[1] in (44100,48000,96000) and fields[2] in (0,1) and fields[3] in (8,16) and fields[4] in (64,256,1024) and fields[5]==4096,'source case grid');count=r.u32();require(count==192,'source case exact control count');rows=[]
 for _ in range(count):
  name,id,bits=text(),r.u32(),r.raw(8);value=struct.unpack('<d',bits)[0];require(math.isfinite(value) and 0<=value<=1,'source case normalized value');rows.append((name,id,bits))
 require([(name,id) for name,id,_ in rows]==list(GENERATED),'case controls differ from source-derived domain')
 require(dict((id,bits) for _,id,bits in rows)[PANEL[0]]==ONE and dict((id,bits) for _,id,bits in rows)[PANEL[1]]==ONE,'case Panel routing required')
 return fields,[(id,bits) for _,id,bits in rows]
def flush_rows(data):
 r=Reader(bytes(data));require(r.text(64)=='cs80-control-input-v1','source flush protocol schema');mode=r.u32();require(mode in (0,2),'source flush mode');count=r.u64();require(count<=192,'source flush control cap');rows=[(r.u32(),r.raw(8)) for _ in range(count)];r.end();require(len({id for id,_ in rows})==len(rows),'source flush unique IDs');require(all(math.isfinite(struct.unpack('<d',bits)[0]) and 0<=struct.unpack('<d',bits)[0]<=1 for _,bits in rows),'source flush normalized value');return mode,rows

def prepare(metadata,initial,alter,case):
 decoded=connected_metadata(bytes(metadata));require(decoded['parameter_count']==2378,'source exact complete native count')
 classification=classify_roster(decoded,[{'name':n,'id':i} for n,i in GENERATED],[{'name':n,'id':i} for n,i in MEMORY])
 require(classification['role_counts']=={'generated_workload_control':192,'source_memory_control':104,'pinned_juce_wrapper_bypass_pattern':1,'pinned_juce_wrapper_program_pattern':1,'pinned_juce_midi_emulation_pattern':2080} and not classification['missing_generated'] and not classification['missing_source_memory'],'source exact role partition')
 expected={row[0]:row for row in expected_metadata()};require(len(expected)==2378,'complete immutable source metadata')
 for p in decoded['parameters']:
  row=expected.get(p['id']);require(row is not None,'unknown source parameter ID');id,role,title,short,units,steps,default,unit,flags=row
  require(all(p[key]['utf16le_hex']==value.encode('utf-16-le').hex() for key,value in (('title',title),('short_title',short),('units',units))) and p['step_count_raw']==(steps&0xffffffff) and p['default_f64le_hex']==default and p['unit_id_raw']==(unit&0xffffffff) and p['flags_raw']==flags,'exact source metadata fields differ')
  if role!=0:require(p['current_f64le_hex']==default,'source residual baseline differs from immutable default')
 generated=set(IDS.values());residual=[]
 for p,role in zip(decoded['parameters'],classification['rows']):
  if p['id'] in generated:require(p['flags_raw']==1,'source workload automation flags')
  else:
   if role['role']=='source_memory_control':require(p['flags_raw']==1,'source memory automation flags')
   else:require(p['current_normalized']==0,'initial wrapper/proxy value must be zero')
   residual.append((p['id'],bytes.fromhex(p['current_f64le_hex'])))
 require(len(residual)==2186,'source residual count');fields,controls=case_controls(case);mode,actual=flush_rows(initial);alter_mode,alter_rows=flush_rows(alter)
 require(mode==(0 if fields[2]==0 else 2) and alter_mode==mode,'source flush/case mode agreement')
 require(actual==controls,'initial writes must exactly equal source192 case controls')
 require(alter_rows==[(id,struct.pack('<Q',bits)) for id,bits in ALTER],'alter writes must exactly equal source2 controls')
 return residual,{'complete_parameter_count':2378,'source_role_counts':classification['role_counts'],'connected_metadata_sha256':decoded['metadata_sha256'],'initial_write_count':192,'alter_write_count':2,'residual_write_count':0,'native_full_metadata_conformance_still_required':True}

def readback(data,stage,residual):
 r=Reader(bytes(data));require(r.text(64)=='cs80-observe-only-readback-v1' and r.u32()==stage,'source readback stage/schema');require(r.u64()==2188,'source readback exact count');rows=[(r.u32(),r.raw(8)) for _ in range(2188)];r.end()
 require(rows==residual+[(PANEL[0],ONE),(PANEL[1],ONE)],'source residual baseline/Panel readback mismatch')
 return hashlib.sha256(bytes(data)).hexdigest()
