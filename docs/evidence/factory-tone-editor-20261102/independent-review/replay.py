import collections, hashlib, json, os, pathlib, re, subprocess, sys
B=pathlib.Path('/workspace/scratch/aa102d573ba3')
S=B/'cs80-tone-editor'
E=B/'cs80-host-tool-evidence/factory-tone-editor'
A=S/'docs/evidence/factory-tone-editor-20261102/compiled-controls'
O=pathlib.Path('/tmp/cs80-independent-editor-d04')
O.mkdir(exist_ok=True)
def h(p): return hashlib.sha256(pathlib.Path(p).read_bytes()).hexdigest()
def git(*args): return subprocess.check_output(['git','-C',str(S),*args]).strip()
def j(p): return json.loads(pathlib.Path(p).read_text())
positive=j(E/'green-corrected/receipt.json')
summary=j(A/'summary.json')
name=sys.argv[1]
assert git('rev-parse','HEAD').decode()=='d04c20acc4fd7089deff7a4b34f02db5dcecec78'
assert not git('status','--porcelain')
assert git('rev-parse','HEAD:src').decode()==positive['source_tree']==summary['source_tree']
for p,d in positive['inputs'].items(): assert h(S/p)==d,(p,d)
for p,d in positive['inputs'].items():
    assert hashlib.sha256(subprocess.check_output(['git','-C',str(S),'show',positive['head']+':'+p])).hexdigest()==d,p
record=next(x for x in summary['cases'] if x['name']==name)
receipt=E/'qualification'/name/'receipt.json'
assert h(receipt)==record['receipt_sha256']
assert receipt.read_bytes()==(A/name/'receipt.json').read_bytes()
r=j(receipt)
assert r['positive_receipt_sha256']==h(E/'green-corrected/receipt.json')
assert r['test_source_sha256']==h(S/'tests/FactoryToneRevisionUiTests.h')
assert r['head']==positive['head'] and r['source_tree']==positive['source_tree']
binary=E/'green-corrected/components' if name.startswith('positive-') else E/'qualification'/name/'components'
assert h(binary)==r['binary_sha256']==record['binary_sha256']
assert len(r['linked_objects'])==45
for row in r['linked_objects']: assert h(row['path'])==row['sha256'],row['path']
link=positive['link_argv'] if name.startswith('positive-') else r['link_argv']
build=B/'cs80-panel-s2/build-native-red'
actual=[]
for arg in link:
    p=pathlib.Path(arg)
    if not p.is_absolute():p=build/p
    if p.is_file() and p.suffix in ('.o','.a'): actual.append({'path':str(p),'sha256':h(p)})
assert actual==r['linked_objects']
if not name.startswith('positive-'):
    original=(S/'src/plugin/AdvancedPanel.cpp').read_text()
    assert hashlib.sha256(original.encode()).hexdigest()==r['original_source_sha256']
    assert original.count(r['replacement_before'])==1
    reconstructed=original.replace(r['replacement_before'],r['replacement_after'],1).encode()
    cpp=E/'qualification'/name/'AdvancedPanel.cpp'
    assert reconstructed==cpp.read_bytes() and h(cpp)==r['mutant_source_sha256']
    assert h(cpp.with_suffix('.o'))==r['mutant_object_sha256']
    expected_link=[str(cpp.with_suffix('.o')) if a==positive['unchanged_advanced_object'] else a for a in positive['link_argv']]
    expected_link[expected_link.index('-o')+1]=str(binary)
    expected_link=[('-Wl,--dependency-file='+str(cpp.parent/'link.d')) if a.startswith('-Wl,--dependency-file=') else a for a in expected_link]
    assert expected_link==r['link_argv']
env=os.environ.copy();env.pop('DISPLAY',None)
env['XDG_CACHE_HOME']=str(O/name/'cache');env['XDG_CONFIG_HOME']=str(O/name/'config')
(O/name).mkdir(exist_ok=True)
p=subprocess.run([str(binary)],env=env,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=150)
(O/name/'run.log').write_text(p.stdout)
failures=[x[6:] for x in p.stdout.splitlines() if x.startswith('FAIL: ')]
assert re.findall(r'Factory tone editor component: (\d+) checks, (\d+) failures; desktop components0',p.stdout)==[('56',str(record['failures']))]
assert p.returncode==r['exit_code']==(0 if name.startswith('positive-') else 1)
assert collections.Counter(failures)==collections.Counter(r['failures'])
assert len(failures)==record['failures']
assert 'NOT RUN' not in p.stdout
for row in r['linked_objects']: assert h(row['path'])==row['sha256']
assert h(binary)==r['binary_sha256']
print(name,'PASS',56,'checks',len(failures),'exact ordinary failures;',len(actual),'linked objects',flush=True)
