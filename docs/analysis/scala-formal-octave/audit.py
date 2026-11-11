from pathlib import Path
import json, re
root=Path(__file__).resolve().parent
steps=[0.,100.,300.,500.,800.]
def cents(degree):
    q,r=divmod(degree,5)
    return q*1200.+steps[r]
def rows(path):
    out={}
    for line in path.read_text().splitlines():
        m=re.match(r'\s*(\d+):\s*([0-9]+\.[0-9]+)\s*[^0-9.]',line)
        if m: out[int(m[1])]=float(m[2])
    assert set(out)==set(range(128)), (path,len(out))
    return out
observed=rows(root/'formal3-oracle.txt')
match_degree=match_fixed=0
comparison=[]
for n,hz in observed.items():
    rep,index=divmod(n-60,2)
    expected=440.*2**(cents(rep*3+index)/1200.)
    fixed=440.*2**((rep*500.+steps[index])/1200.)
    # Scala formats roughly six decimal places, reducing precision for large Hz.
    tol=1e-6+1e-10*max(hz,expected)
    match_degree+=abs(hz-expected)<=tol
    match_fixed+=abs(hz-fixed)<=tol
    if 54<=n<=68:
        comparison.append(dict(midi=n,degree=rep*3+index,scala_hz=hz,degree_first_hz=expected,fixed_repeat_hz=fixed))
zero=rows(root/'formal0-oracle.txt')
assert all(abs(hz-(440. if n%2==0 else 440*2**(100/1200)))<1e-6 for n,hz in zero.items())
# The software is intentionally not redistributed; hashes in audit.json are
# provenance records, not reverified by this output-only audit.
record=json.loads((root/'audit.json').read_text())
assert record['degree_first_matches']==match_degree==128
assert record['fixed_repeat_matches']==match_fixed==3
assert record['comparison']==comparison
print(json.dumps(dict(degree_first_matches=match_degree,fixed_repeat_matches=match_fixed,total=128,zero_observation_matches=True),indent=2))
