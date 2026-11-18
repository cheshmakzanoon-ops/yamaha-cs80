from pathlib import Path
import argparse,struct,json,hashlib,subprocess
p=argparse.ArgumentParser(description='Compare non-batch Engine machine bytes and resolved relocations; no DSP execution.')
p.add_argument('stock_object',type=Path)
p.add_argument('candidate_object',type=Path)
p.add_argument('output_json',type=Path)
args=p.parse_args()
name='.text._ZN4cs806Engine25processInternalSampleImplILb0EEEvRfS2_f'
def sections(path):
 b=path.read_bytes();assert b[:6]==b'\x7fELF\x02\x01';h=struct.unpack_from('<HHIQQQIHHHHHH',b,16);off=h[5];entry=h[10];n=h[11];stridx=h[12]
 rows=[struct.unpack_from('<IIQQQQIIQQ',b,off+i*entry) for i in range(n)];st=rows[stridx];strings=b[st[4]:st[4]+st[5]]
 return {strings[r[0]:strings.index(0,r[0])].decode():b[r[4]:r[4]+r[5]] for r in rows if r[1]!=8}
a=sections(args.stock_object)[name];b=sections(args.candidate_object)[name]
# Objdump includes symbolic relocation targets; only the banner contains the input filename.
da=subprocess.check_output(['objdump','-dr','-j',name,str(args.stock_object)],text=True).split('Disassembly of section ',1)[1]
db=subprocess.check_output(['objdump','-dr','-j',name,str(args.candidate_object)],text=True).split('Disassembly of section ',1)[1]

r={'section':name,'stock_bytes':len(a),'observed_bytes':len(b),'machine_bytes_identical':a==b,'stock_sha256':hashlib.sha256(a).hexdigest(),'observed_sha256':hashlib.sha256(b).hexdigest(),'disassembly_with_symbolic_relocations_identical':da==db}
(args.output_json).write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
def relocations(path,target):
 data=path.read_bytes();h=struct.unpack_from('<HHIQQQIHHHHHH',data,16);off,entry,n,stridx=h[5],h[10],h[11],h[12]
 sh=[struct.unpack_from('<IIQQQQIIQQ',data,off+i*entry) for i in range(n)]
 z=sh[stridx];sn=data[z[4]:z[4]+z[5]]
 secname=lambda i:sn[sh[i][0]:sn.index(0,sh[i][0])].decode()
 get=lambda i:data[sh[i][4]:sh[i][4]+sh[i][5]]
 idx=next(i for i in range(n) if secname(i)=='.rela'+target);rs=sh[idx];syidx=rs[6];sy=sh[syidx];st=get(sy[6]);syms=[]
 for q in range(0,sy[5],sy[9]):
  v=struct.unpack_from('<IBBHQQ',data,sy[4]+q);name=st[v[0]:st.index(0,v[0])].decode();syms.append((name,*v[1:]))
 out=[]
 for q in range(0,rs[5],rs[9]):
  pos,info,add=struct.unpack_from('<QQq',data,rs[4]+q);symbol=syms[info>>32];name,flags,other,section,value,size=symbol
  if name.startswith('.LC'):
   step=sh[section][9];assert step in (4,8,16), (name,step)
   key=('constant',secname(section),get(section)[value:value+step].hex())
  else:key=('symbol',name or (secname(section) if section<n else ''),value if (flags&15)==3 else 0)
  out.append((pos,info&0xffffffff,key,add))
 return out
ra=relocations(args.stock_object,name);rb=relocations(args.candidate_object,name)
r['relocation_count']=len(ra);r['relocations_equal_after_resolving_constant_bytes']=ra==rb
if ra!=rb:r['relocation_difference_count']=sum(x!=y for x,y in zip(ra,rb))+abs(len(ra)-len(rb))
(args.output_json).write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({k:v for k,v in r.items() if k.startswith('relocation') and k!='relocation_differences'},indent=2))

raise SystemExit(0 if r['machine_bytes_identical'] and r['relocations_equal_after_resolving_constant_bytes'] else 1)
