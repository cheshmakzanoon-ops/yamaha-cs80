import pathlib,json,re,hashlib,subprocess,collections
root=pathlib.Path(__file__).resolve().parents[1]; out=root/'hybrid-full-continuation-evidence'; prior=root/'hybrid-full-rerun-evidence'
def results(p):
 t=p.read_text(); return re.findall(r'Test\s+#\d+:\s+(\S+)\s+\.+\s*(Passed|\*\*\*Failed)',t)
paths=[prior/'nonwindow.log',prior/'lfo-fresh-retest.log',out/'ctest.log']
rs=[results(p) for p in paths]; passes=[[n for n,s in r if s=='Passed'] for r in rs]
inv=json.loads((prior/'nonwindow-inventory.json').read_text()); names={t['name'] for t in inv['tests']}
claimed=json.loads((out/'combined-coverage.json').read_text()); flat=sum(passes,[])
assert list(map(len,passes))==[295,2,260]; assert set(flat)==names and len(flat)==len(set(flat))==557
for k,v in zip(['interrupted','fresh_lfo_retest','continuation'],passes): assert v==claimed['segments'][k]
allinv=json.loads((root/'hybrid-full-evidence/test-inventory.json').read_text()); allnames={t['name'] for t in allinv['tests']}; exclusions=sorted(allnames-names); assert len(allnames)==564 and len(exclusions)==7
pat=(prior/'nonwindow-exclusion.txt').read_text().strip(); assert {n for n in allnames if re.search(pat,n)}==set(exclusions)
hashes=json.loads((prior/'production-source-identities.json').read_text()); mismatches=[p for p,h in hashes.items() if hashlib.sha256((root/'audio-root-hybrid'/p).read_bytes()).hexdigest()!=h]; assert len(hashes)==192 and not mismatches
assert (out/'ctest-exit.txt').read_text().strip()=='0'; assert (prior/'lfo-fresh-retest-exit.txt').read_text().strip()=='0'
assert 'Total Test time (real) = 6013.86 sec' in paths[2].read_text()
assert '100% tests passed out of 260' in paths[2].read_text()
tmp=root/'hybrid-full-build/Testing/Temporary/LastTest.log.tmpf912a'; saved=out/'interrupted-LastTest.log'; saved.write_bytes(tmp.read_bytes())
last=out/'LastTest.log'; assert last.read_bytes()==(root/'hybrid-full-build/Testing/Temporary/LastTest.log').read_bytes()
report={'pass_counts':list(map(len,passes)),'prior_failures':[n for n,s in rs[0] if s!='Passed'],'union':len(set(flat)),'exclusions':exclusions,'production_files_verified':len(hashes),'head':subprocess.check_output(['git','-C',str(root/'audio-root-hybrid'),'rev-parse','HEAD'],text=True).strip(),'logs_sha256':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in paths+[saved,last]},'single_green_run':False}
(out/'independent-audit.json').write_text(json.dumps(report,indent=2)+'\n'); print(json.dumps(report,indent=2))
