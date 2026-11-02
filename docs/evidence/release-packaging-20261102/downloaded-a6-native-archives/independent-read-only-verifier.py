import hashlib,io,json,pathlib,posixpath,struct,subprocess,tarfile,urllib.request,zipfile
BASE=pathlib.Path('/workspace/scratch/aa102d573ba3/cs80-release-artifacts/a6e5685/native-install-bundles')
REPO=pathlib.Path('/workspace/scratch/aa102d573ba3/yamaha-cs80')
SOURCE='a6e568543f3d194f10f75af2216bd1dc686ff3e5'
JUCE='c352e2489000ffeb9e397051377d75eef2efe641'
META={x['id']:x for x in json.load(open('/tmp/cs80-a6-final-artifacts.json'))['artifacts']}
INPUTS={'LICENSE':'LICENSE','licenses/MTS-ESP-LICENSE.txt':'deps/mts-esp/LICENSE','licenses/BarlowCondensed-OFL.txt':'assets/fonts/barlow-condensed/OFL.txt','licenses/BarlowCondensed-SOURCE.txt':'assets/fonts/barlow-condensed/SOURCE.txt','INSTALL_AND_QUICKSTART.md':'docs/INSTALL_AND_QUICKSTART.md','DISTRIBUTION_STATUS.md':'docs/CI_DISTRIBUTION_STATUS.md'}
sha=lambda b:hashlib.sha256(b).hexdigest()
expected={name:subprocess.check_output(['git','-C',str(REPO),'show',SOURCE+':'+path]) for name,path in INPUTS.items()}
url='https://raw.githubusercontent.com/juce-framework/JUCE/'+JUCE+'/LICENSE.md'
with urllib.request.urlopen(url,timeout=30) as r:expected['licenses/JUCE-LICENSE.md']=r.read()
(BASE/'JUCE-LICENSE-c352e248.md').write_bytes(expected['licenses/JUCE-LICENSE.md'])
expected['SOURCE.txt']=(f'Packaging source checkout: {SOURCE}\nhttps://github.com/cheshmakzanoon-ops/yamaha-cs80/tree/{SOURCE}\nUse the complete source and build instructions at this exact revision.\nThis package is not a source distribution.\nCheckout provenance does not independently establish binary/source equivalence,\nnumerical acceptance, hardware qualification, signing or real-DAW acceptance.\n').encode()
records=[]
for id,name,platform in [(13135277790,'cs80-a6-linux-install-bundle.zip','linux'),(13138005606,'cs80-a6-windows-install-bundle.zip','windows'),(13141651235,'cs80-a6-macos-intel-install-bundle.zip','macos')]:
 outer=BASE/name;blob=outer.read_bytes();assert 'sha256:'+sha(blob)==META[id]['digest'];assert len(blob)==META[id]['size_in_bytes']
 prefix=f'cs80-reference-{platform}-x86_64-{SOURCE[:12]}';archive=prefix+('.zip' if platform=='windows' else '.tar.gz');manifest_name=prefix+'.manifest.json'
 with zipfile.ZipFile(io.BytesIO(blob)) as z:
  assert z.testzip() is None and sorted(z.namelist())==sorted(['SHA256SUMS',archive,manifest_name])
  raw_manifest=z.read(manifest_name);m=json.loads(raw_manifest);payload=z.read(archive);sums=z.read('SHA256SUMS').decode().splitlines()
  assert sorted(sums)==sorted([sha(payload)+'  '+archive,sha(raw_manifest)+'  '+manifest_name])
 assert m['commit']==SOURCE and m['workflow_run_id']=='41427704633' and m['platform']==platform and m['architecture']=='x86_64'
 assert m['juce_sha']==JUCE and m['juce_version']=='8.0.11' and m['package_kind']=='development CI artifact; not a release-qualification certificate'
 entries={x['path']:x for x in m['files']};assert len(entries)==len(m['files'])
 seen=set();contents={};kinds={};modes={};targets={}
 def add(name,kind,content,mode,target=''):
  name=name.rstrip('/');parts=pathlib.PurePosixPath(name).parts
  assert parts and name.startswith(prefix+'/') and '..' not in parts and '\\' not in name and ':' not in parts[0] and name not in seen
  seen.add(name);relative=name[len(prefix)+1:]
  if relative=='manifest.json':assert content is not None and json.loads(content)==m;return
  assert relative in entries;row=entries[relative];assert row['type']==kind
  if kind=='file':assert len(content)==row['size'] and sha(content)==row['sha256'];contents[relative]=content
  if kind=='symlink':
   assert target==row['target'] and not target.startswith('/')
   resolved=posixpath.normpath(posixpath.join(posixpath.dirname(name),target));assert resolved.startswith(prefix+'/')
   targets[relative]=target
  if platform!='windows':assert mode==int(row['mode'],8)
  kinds[relative]=kind;modes[relative]=mode
 if platform=='windows':
  with zipfile.ZipFile(io.BytesIO(payload)) as z:
   assert z.testzip() is None
   for x in z.infolist():add(x.filename,'directory' if x.is_dir() else 'file',None if x.is_dir() else z.read(x), (x.external_attr>>16)&0o7777)
 else:
  with tarfile.open(fileobj=io.BytesIO(payload),mode='r:gz') as t:
   for x in t:
    assert x.isfile() or x.isdir() or x.issym()
    add(x.name,'file' if x.isfile() else 'directory' if x.isdir() else 'symlink',t.extractfile(x).read() if x.isfile() else None,x.mode,x.linkname)
 assert seen=={prefix+'/'+p for p in entries}|{prefix+'/manifest.json'}
 notices=[]
 for key,reference in expected.items():
  data=contents[key]
  conversion='exact Git/source bytes'
  if data!=reference:
   assert platform=='windows' and b'\r' not in reference and data==reference.replace(b'\n',b'\r\n'),key
   conversion='observed LF-to-CRLF checkout conversion only; not raw Git-blob byte identity'
  notices.append({'path':key,'bytes':len(data),'sha256':sha(data),'source_bytes_sha256':sha(reference),'comparison':conversion})
 provenance=dict(line.split('=',1) for line in contents['evidence/juce-provenance.txt'].decode().splitlines() if '=' in line)
 assert provenance['verification']=='PASS' and provenance['source_clean']=='true' and provenance['actual_sha']==JUCE and provenance['actual_version']=='8.0.11' and provenance['workflow_commit']==SOURCE
 binaries=([f'VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so','Standalone/CS-80 Reference'] if platform=='linux' else [f'VST3/CS-80 Reference.vst3/Contents/x86_64-win/CS-80 Reference.vst3','Standalone/CS-80 Reference.exe'] if platform=='windows' else [f'{d}/CS-80 Reference{suffix}/Contents/MacOS/CS-80 Reference' for d,suffix in [('VST3','.vst3'),('AU','.component'),('Standalone','.app')]])
 binary_records=[]
 for key in binaries:
  b=contents[key];assert b
  if platform=='linux':assert b[:5]==b'\x7fELF\x02' and b[5]==1 and struct.unpack_from('<H',b,18)[0]==62;format_name='ELF64 x86_64'
  elif platform=='windows':
   assert b[:2]==b'MZ';offset=struct.unpack_from('<I',b,60)[0];assert b[offset:offset+4]==b'PE\0\0' and struct.unpack_from('<H',b,offset+4)[0]==0x8664;format_name='PE x86_64'
  else:assert b[:4]==b'\xcf\xfa\xed\xfe' and struct.unpack_from('<I',b,4)[0]==0x01000007;format_name='Mach-O64 x86_64, not universal'
  binary_records.append({'path':key,'format':format_name,'bytes':len(b),'sha256':sha(b)})
 (BASE/(platform+'-manifest.json')).write_bytes(raw_manifest)
 records.append({'artifact_id':id,'outer_file':name,'outer_sha256':sha(blob),'package_file':archive,'package_sha256':sha(payload),'manifest_sha256':sha(raw_manifest),'archive_entries':len(entries),'notices':notices,'native_binary_headers':binary_records,'safe_internal_symlinks':targets,'source_commit':SOURCE,'workflow_run_id':'41427704633','qualification':'Downloaded development archive inventory/bytes, notice source comparison and file headers only. No binary execution, installer install/uninstall, signature validation, compiler/source equivalence, musical acceptance or release.'})
 print(platform,'PASS',len(entries),'members',len(notices),'source notices',len(binaries),'native binary headers')
receipt={'schema_version':1,'source_commit':SOURCE,'workflow_run_id':'41427704633','overall_run':'failure: ARM job cancelled; these three native jobs succeeded','juce_license_primary_url':url,'artifacts':records,'scope':'Bounded downloaded CI archive verification. Does not execute or qualify the later unpublished native installer increment.'}
(BASE/'downloaded-archive-verification.json').write_text(json.dumps(receipt,indent=2)+'\n')
