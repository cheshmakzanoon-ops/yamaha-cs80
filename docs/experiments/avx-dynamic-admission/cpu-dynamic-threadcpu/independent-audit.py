import pathlib,csv,statistics,json,hashlib
base=pathlib.Path('/workspace/scratch/ee447c520988/cs80-restored')
for dn in ['cpu-avx-dynamic-pairs','cpu-dynamic-wide','cpu-dynamic-threadcpu','cpu-dynamic-idle-threadcpu']:
 p=base/dn; print('\n'+dn)
 for x in sorted(p.glob('*-baseline')):
  y=p/(x.name[:-8]+'candidate')
  if not (x/'results.csv').exists() or not(y/'results.csv').exists():continue
  a=list(csv.DictReader(open(x/'results.csv')));b=list(csv.DictReader(open(y/'results.csv')))
  key=lambda r:tuple(r[k] for k in ['mode','scenario','initial_selector','phase'])
  aa={key(r):r for r in a};bb={key(r):r for r in b}
  assert len(a)==len(aa) and len(b)==len(bb) and aa.keys()==bb.keys()
  assert all(aa[k]['checksum']==bb[k]['checksum'] for k in aa)
  assert all(int(r['nonfinite_count'])==0 for r in a+b)
  metric='callback_cpu_rtf' if 'callback_cpu_rtf' in a[0] else 'mean_rtf'
  rs={k:float(bb[k][metric])/float(aa[k][metric]) for k in aa if float(aa[k][metric])>0}
  ks=[k for k in rs if k[0]=='source_performance' and k[3]=='warmed_held']
  print(x.name,len(a),metric,'sourceheld',statistics.median(rs[k] for k in ks) if ks else None)
  print('worst',sorted(rs.items(),key=lambda t:t[1],reverse=True)[:3])
  for k in rs:
   if k[:2]==('isolation','idle_quiet'):print('quiet',rs[k])
for dn in ['cpu-dynamic-threadcpu','cpu-dynamic-idle-threadcpu']:
 for line in (base/dn/'input-hashes.txt').read_text().splitlines():
  h,f=line.split();assert hashlib.sha256((base.parent/f).read_bytes()).hexdigest()==h
print('binary hashes verified')
