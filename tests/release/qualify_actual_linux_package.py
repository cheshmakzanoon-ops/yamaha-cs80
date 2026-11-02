#!/usr/bin/env python3
"""Preregistered actual Linux transport exercise; never execute a payload binary."""
import argparse,hashlib,importlib.util,io,json,os,platform,re,signal,stat,struct,subprocess,sys,tarfile,time,zipfile
from pathlib import Path
BINARY_SOURCE='a6e568543f3d194f10f75af2216bd1dc686ff3e5'
CODE_ANCHOR='e55540b098af022137c1f90582386673b683bca2'
OUTER_SHA='40be0750f5a5606bd6c90decf37b83c5b880a5c35633e8ff277e70b6f69814ee'
JUCE_SHA='2633539bb26d244f0966fbc4df59400ea99bdf575fa96291851c1c3ff3456146'
CODE=('scripts/distribution_docs.py','scripts/verify_distribution_docs.py','scripts/verify_installer_binaries.py','scripts/qualify_native_installer.py','installer/linux/make-deb.sh')
NOTICES={'LICENSE':'LICENSE','licenses/MTS-ESP-LICENSE.txt':'deps/mts-esp/LICENSE','licenses/BarlowCondensed-OFL.txt':'assets/fonts/barlow-condensed/OFL.txt','licenses/BarlowCondensed-SOURCE.txt':'assets/fonts/barlow-condensed/SOURCE.txt','INSTALL_AND_QUICKSTART.md':'docs/INSTALL_AND_QUICKSTART.md'}
def sha(data):return hashlib.sha256(data).hexdigest()
def require(ok,reason):
 if not ok:raise ValueError(reason)
def imported(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m

def main():
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--source-root',type=Path,required=True);p.add_argument('--bundle',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args()
 root=a.source_root.resolve();out=a.out.resolve()
 require(not out.is_relative_to(root), 'native exercise output must be outside the source checkout')
 out.mkdir(parents=True,exist_ok=False)
 def git(*args):return subprocess.check_output(['git','-C',str(root),*args])
 revision=git('rev-parse','HEAD').decode().strip();source_tree=git('rev-parse','HEAD:src').decode().strip()
 record={'status':'incomplete','packaging_source':revision,'packaging_source_tree':source_tree,'packaging_full_tree':git('rev-parse','HEAD^{tree}').decode().strip(),'packaging_code_anchor':CODE_ANCHOR,'binary_origin_source':BINARY_SOURCE,'binary_artifact_id':13135277790,'binary_workflow_run':41427704633,'runner_sha256':sha(Path(__file__).read_bytes()),'payload_binary_executions':0,'system_installations':0,'source_publication':'packaging source is local only; no remote availability is assumed','controls':[],'scope':'Actual native Linux Debian container transport only; not compilation/source equivalence, audio, CPU, host, signing or release qualification'}
 def save():(out/'qualification.json').write_text(json.dumps(record,indent=2)+'\n')
 def run(args,log):
  with (out/log).open('x') as stream:
   stream.write('command='+json.dumps([str(v)for v in args])+'\n');stream.flush()
   # Kill only this command's own process group on timeout/failure, retaining the
   # caller's shared lock through cleanup; do not orphan dpkg/compression children.
   child=subprocess.Popen([str(v)for v in args],stdout=stream,stderr=subprocess.STDOUT,cwd=root,start_new_session=True)
   def cleanup():
    for sig,delay in ((signal.SIGTERM,0.5),(signal.SIGKILL,0)):
     try:os.killpg(child.pid,sig)
     except ProcessLookupError:break
     if delay:time.sleep(delay)
    child.wait()
   try:
    code=child.wait(timeout=180)
    if code:
     raise subprocess.CalledProcessError(code,args)
   except BaseException:
    cleanup();raise
 save()
 try:
  require(sys.platform.startswith('linux') and platform.machine()=='x86_64','native Linux x86_64 required')
  require(git('status','--porcelain=v1','--untracked-files=all')==b'','packaging checkout must be clean')
  for path in CODE:require((root/path).read_bytes()==git('show',CODE_ANCHOR+':'+path),'packaging code differs from preregistered reviewed anchor: '+path)
  require(Path(__file__).resolve()==root/'tests/release/qualify_actual_linux_package.py' and Path(__file__).read_bytes()==git('show',revision+':tests/release/qualify_actual_linux_package.py'),'runner must be the committed actual checkout file')
  require(not os.environ.get('GITHUB_SHA') or os.environ['GITHUB_SHA']==revision,'unrelated workflow SHA cannot be overridden')
  record['tool_observation']={'python':sys.version,'platform':platform.platform(),'dpkg_deb':subprocess.check_output(['dpkg-deb','--version'],text=True).strip(),'architecture':subprocess.check_output(['dpkg','--print-architecture'],text=True).strip()}
  require(record['tool_observation']['architecture']=='amd64','native Debian architecture differs')
  blob=a.bundle.read_bytes();require(len(blob)==12485871 and sha(blob)==OUTER_SHA,'outer artifact size/hash differs')
  prefix='cs80-reference-linux-x86_64-'+BINARY_SOURCE[:12];archive=prefix+'.tar.gz';manifest=prefix+'.manifest.json'
  with zipfile.ZipFile(io.BytesIO(blob)) as z:
   require(z.testzip()is None and sorted(z.namelist())==sorted(['SHA256SUMS',archive,manifest]),'outer artifact CRC/inventory')
   raw=z.read(manifest);m=json.loads(raw);payload=z.read(archive)
   require(sorted(z.read('SHA256SUMS').decode().splitlines())==sorted([sha(raw)+'  '+manifest,sha(payload)+'  '+archive]),'nested SHA256SUMS differ')
  require(m['commit']==BINARY_SOURCE and m['workflow_run_id']=='41427704633' and m['architecture']=='x86_64' and m['platform']=='linux','binary input provenance differs')
  entries={x['path']:x for x in m['files']};require(len(entries)==16 and len(m['files'])==16,'expected16 archived input entries')
  contents={};seen=set();build=out/'build-input';artifacts=build/'CS80Reference_artefacts/Release';artifacts.mkdir(parents=True)
  with tarfile.open(fileobj=io.BytesIO(payload),mode='r:gz') as t:
   for member in t:
    name=member.name.rstrip('/');require(name.startswith(prefix+'/') and '..'not in Path(name).parts and '\\'not in name and name not in seen,'unsafe or duplicate archive path');seen.add(name);relative=name[len(prefix)+1:]
    require(member.isfile()or member.isdir(),'unexpected Linux member type')
    value=t.extractfile(member).read()if member.isfile()else None
    if relative=='manifest.json':require(value is not None and json.loads(value)==m,'embedded input manifest differs');continue
    require(relative in entries,'unregistered input member');row=entries[relative];require(row['type']==('file'if member.isfile()else'directory') and member.mode==int(row['mode'],8),'input type/mode differs')
    if value is not None:require(sha(value)==row['sha256'] and len(value)==row['size'],'input bytes differ');contents[relative]=value
    if relative.startswith(('VST3/','Standalone/')):
     path=artifacts/relative;path.parent.mkdir(parents=True,exist_ok=True)
     if member.isdir():path.mkdir(exist_ok=True)
     else:path.write_bytes(value)
     path.chmod(member.mode)
  require(seen=={prefix+'/'+p for p in entries}|{prefix+'/manifest.json'},'nested input inventory differs')
  require(sha(contents['licenses/JUCE-LICENSE.md'])==JUCE_SHA,'JUCE licence input differs')
  licence=build/'_deps/juce-src/LICENSE.md';licence.parent.mkdir(parents=True);licence.write_bytes(contents['licenses/JUCE-LICENSE.md'])
  record['input_archive_sha256']=sha(payload);record['outer_artifact_sha256']=sha(blob);record['input_manifest_sha256']=sha(raw)
  run([sys.executable,root/'scripts/qualify_native_installer.py','--platform','linux','--build-dir',build,'--out-dir',out/'native','--source-sha',revision],'native-controller.log')
  native=json.loads((out/'native/native-installer-receipt.json').read_text());package=out/'native'/native['package']
  require(package.name.endswith('.deb') and '-CI-UNSIGNED-'+revision[:12]in package.name,'native package label is not bounded CI identity')
  require(sha(package.read_bytes())==native['package_sha256'] and package.stat().st_size==native['package_bytes'],'native package receipt differs')
  for field,expected in [('Package','cs80-reference'),('Architecture','amd64')]:require(subprocess.check_output(['dpkg-deb','--field',str(package),field],text=True).strip()==expected,'native Debian '+field+' differs')
  version=re.search(r'^project\(CS80Reference VERSION ([0-9.]+)',(root/'CMakeLists.txt').read_text(),re.M)[1]
  require(subprocess.check_output(['dpkg-deb','--field',str(package),'Version'],text=True).strip()==version,'native Debian version differs')
  run(['dpkg-deb','--info',package],'native-info.log');expanded=out/'independent-expanded';run(['dpkg-deb','--extract',package,expanded],'independent-extract.log')
  binary_paths={'usr/bin/cs80-reference':'Standalone/CS-80 Reference','usr/lib/vst3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so':'VST3/CS-80 Reference.vst3/Contents/x86_64-linux/CS-80 Reference.so'}
  elf_headers={}
  for actual,original in binary_paths.items():
   value=(expanded/actual).read_bytes();require(value==contents[original],'independent actual binary transport differs')
   require(len(value)>=64 and value[:7]==b'\x7fELF\x02\x01\x01','expected Linux ELF64 little-endian header')
   kind,machine=struct.unpack_from('<HH',value,16)
   require(kind in (2,3) and machine==62,'expected x86_64 ELF executable/shared-object header')
   elf_headers[actual]={'bytes':len(value),'sha256':sha(value),'elf_type':kind,'elf_machine':machine}
  record['independent_elf_headers']=elf_headers
  documents=expanded/'usr/share/doc/cs80-reference'
  for output,source in NOTICES.items():require((documents/output).read_bytes()==git('show',revision+':'+source),'independent raw Git notice mismatch: '+output)
  require((documents/'licenses/JUCE-LICENSE.md').read_bytes()==contents['licenses/JUCE-LICENSE.md'],'independent JUCE notice mismatch')
  source_text=(f'Packaging source checkout: {revision}\nhttps://github.com/cheshmakzanoon-ops/yamaha-cs80/tree/{revision}\nUse the complete source and build instructions at this exact revision.\nThis package is not a source distribution.\nCheckout provenance does not independently establish binary/source equivalence,\nnumerical acceptance, hardware qualification, signing or real-DAW acceptance.\n').encode()
  require((documents/'SOURCE.txt').read_bytes()==source_text and (documents/'copyright').read_bytes()==git('show',revision+':LICENSE'),'independent source/copyright notice mismatch')
  helper=imported('actual_linux_binary_verifier',root/'scripts/verify_installer_binaries.py');verifier=imported('actual_linux_notice_verifier',root/'scripts/verify_distribution_docs.py');captured=json.loads((out/'native/binary-input-manifest.json').read_text())
  def positive():return helper.verify_installed_payload('linux',captured,build,expanded)
  positive();verifier.verify_documents(root,licence,documents,revision)
  def reject(name,call,reason):
   try:call()
   except ValueError as error:
    require(reason in str(error),'wrong rejection reason: '+str(error));record['controls'].append({'name':name,'reason':str(error),'ordinary_rejection':True})
   else:raise AssertionError('real payload control admitted: '+name)
  standalone=expanded/'usr/bin/cs80-reference';original=standalone.read_bytes();mode=stat.S_IMODE(standalone.stat().st_mode)
  try:
   standalone.write_bytes(bytes([original[0]^1])+original[1:]);reject('actual-standalone-byte',positive,'Binary payload bytes/hash mismatch')
  finally:standalone.write_bytes(original)
  positive()
  try:
   standalone.chmod(0o644);reject('actual-standalone-mode',positive,'Binary payload mode mismatch')
  finally:standalone.chmod(mode)
  positive();reject('wrong-packaging-source',lambda:verifier.verify_documents(root,licence,documents,'0'*40),'Expected source commit differs from checkout')
  require(git('rev-parse','HEAD').decode().strip()==revision and git('status','--porcelain=v1','--untracked-files=all')==b'','packaging checkout changed during check')
  for path in CODE:require((root/path).read_bytes()==git('show',CODE_ANCHOR+':'+path),'packaging code changed during exercise: '+path)
  record.update(status='pass',native_package=package.name,native_package_sha256=sha(package.read_bytes()),independent_binary_comparisons=2,independent_required_notices=7,restored_positive=True,files={str(p.relative_to(out)):sha(p.read_bytes())for p in sorted((out/'native').glob('*.json'))})
  record['files'].update({p.name:sha(p.read_bytes())for p in out.glob('*.log')});save();print(json.dumps(record,indent=2))
 except BaseException as error:
  record.update(status='failed',error=type(error).__name__+': '+str(error));save();raise
if __name__=='__main__':main()
