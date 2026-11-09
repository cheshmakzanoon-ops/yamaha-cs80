import lzma, pathlib, re, subprocess, hashlib, concurrent.futures, json
base=pathlib.Path(__file__).resolve().parent
records={}
for stanza in lzma.open(base/'Packages.xz','rt').read().split('\n\n'):
 d={}
 for line in stanza.splitlines():
  if line and not line[0].isspace() and ': ' in line:
   k,v=line.split(': ',1); d[k]=v
 if 'Package' in d: records[d['Package']]=d
roots='libasound2-dev libx11-dev libxext-dev libxrandr-dev libxcursor-dev libxinerama-dev libxrender-dev libxcomposite-dev libgl1-mesa-dev libfreetype-dev libfontconfig1-dev'.split()
selected={}
def add(name):
 if name in selected:return
 if name not in records: print('Missing',name,flush=True);return
 d=records[name]; selected[name]=d
 for dep in (d.get('Depends','')+','+d.get('Pre-Depends','')).split(','):
  opts=[re.split(r'[\s(:]',x.strip())[0] for x in dep.split('|')]
  for n in opts:
   if n in records: add(n);break
for n in roots:add(n)
(base/'package-manifest.json').write_text(json.dumps([{k:d.get(k) for k in ['Package','Version','Filename','SHA256']} for d in selected.values()],indent=2))
print('Downloading',len(selected),'packages',flush=True)
def fetch(d):
 p=base/'debs'/pathlib.Path(d['Filename']).name
 if not p.exists():subprocess.run(['curl','-fsSL','--max-time','120','https://deb.debian.org/debian/'+d['Filename'],'-o',str(p)],check=True)
 assert hashlib.sha256(p.read_bytes()).hexdigest()==d['SHA256']
 return p
with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool: paths=list(pool.map(fetch,selected.values()))
for p in paths:subprocess.run(['dpkg-deb','-x',str(p),str(base/'sysroot')],check=True)
print('Extracted',len(paths),'verified packages',flush=True)
