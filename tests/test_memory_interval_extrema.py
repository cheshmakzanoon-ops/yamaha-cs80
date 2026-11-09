#!/usr/bin/env python3
"""Exact-Fraction oracle for interval multiplication/division endpoint selection.

The oracle computes all four exact products/quotients, not the DUT sign cases.
Each extremum is correctly rounded by Python's Fraction-to-float conversion,
then expanded with nextafter. Overflow, invalid domains and signed-zero effects
are therefore checked as well as containment. Optional multiple executables
must serialize byte-identical status/endpoints (NaN payloads are not encoded).
"""
from fractions import Fraction as F
from pathlib import Path
import hashlib, math, random, struct, subprocess, sys
rng=random.Random(0x80e7e940)
tiny=float.fromhex('0x0.0000000000001p-1022')
normal=float.fromhex('0x1p-1022')
maximum=float.fromhex('0x1.fffffffffffffp1023')
intervals=[[-0.,0.],[0.,0.],[-0.,-0.],[-tiny,tiny]]
for value in [tiny,2*tiny,math.nextafter(normal,0.),normal,math.ldexp(1.,-500),
              math.nextafter(1.,0.),1.,math.nextafter(1.,math.inf),
              math.ldexp(1.,500),maximum*.5,maximum]:
    upper=math.nextafter(value,math.inf)
    if not math.isfinite(upper): upper=value
    intervals += [[value,upper],[-upper,-value],[0.,value],[-value,-0.],[-value,value]]

cases=[]
def add(kind,a,b):
    cases.append((kind,list(a),list(b)))
for a in intervals:
    for b in intervals:
        for kind in ['mul','div']: add(kind,a,b)

def finite():
    bits=rng.randrange(0,0x7ff0000000000000)
    value=struct.unpack('>d',struct.pack('>Q',bits))[0]
    return rng.choice([-1.,1.])*value
for _ in range(2048):
    a=sorted([finite(),finite()]);b=sorted([finite(),finite()])
    for kind in ['mul','div']: add(kind,a,b)
# Finite moderate sign combinations all require success, not just rejection
# compatibility at deliberately overflowing extremes.
for sign_a in [-1,0,1]:
    for sign_b in [-1,0,1]:
        for _ in range(64):
            def pair(sign):
                x,y=sorted([math.ldexp(rng.uniform(.5,1.),rng.randrange(-200,201)),
                            math.ldexp(rng.uniform(.5,1.),rng.randrange(-200,201))])
                return [x,y] if sign>0 else [-y,-x] if sign<0 else [-x,y]
            a,b=pair(sign_a),pair(sign_b)
            for kind in ['mul','div']:add(kind,a,b)
for bad in [math.nan,math.inf,-math.inf]:
    for kind in ['mul','div']:
        for a,b in [([bad,1.],[1.,2.]),([1.,2.],[1.,bad]),([2.,1.],[1.,2.])]:add(kind,a,b)

def expected(kind,a,b):
    if not all(map(math.isfinite,a+b)) or a[0]>a[1] or b[0]>b[1]:return None
    if kind=='div' and b[0]<=0<=b[1]:return None
    aa=list(map(F.from_float,a));bb=list(map(F.from_float,b))
    values=[x*y if kind=='mul' else x/y for x in aa for y in bb]
    try:
        lo=math.nextafter(float(min(values)),-math.inf)
        hi=math.nextafter(float(max(values)),math.inf)
    except OverflowError:return None
    return (lo,hi) if math.isfinite(lo) and math.isfinite(hi) else None
truth=[expected(*case) for case in cases]
commands=''.join(kind+' '+' '.join(x.hex() for x in a+b)+'\n' for kind,a,b in cases)
assert len(sys.argv)>1, 'give compiled primitive probe(s)'
reference=None
for name in sys.argv[1:]:
    exe=Path(name).resolve()
    result=subprocess.run([str(exe)],input=commands,text=True,capture_output=True,check=True)
    lines=result.stdout.splitlines();assert len(lines)==len(cases)
    failures=[];accepted=rejected=0
    for index,(line,want) in enumerate(zip(lines,truth)):
        status,left,right=line.split();lo,hi=float.fromhex(left),float.fromhex(right)
        if want is None:
            rejected+=1
            if status!='0' or (math.isfinite(lo) and math.isfinite(hi) and lo<=hi):
                failures.append((index,cases[index],line,'required rejection missing'))
        else:
            accepted+=1
            if status!='1' or struct.pack('>dd',lo,hi)!=struct.pack('>dd',*want):
                failures.append((index,cases[index],line,'exact extremal endpoints differ',want))
    print('Executable:',exe)
    print('Exact-Fraction extrema cases:',len(cases),'accepted:',accepted,'required rejections:',rejected,'failures:',len(failures))
    print('Serialized SHA256:',hashlib.sha256(result.stdout.encode()).hexdigest())
    for failure in failures[:4]:print('FAIL',failure)
    if exe.name.startswith('mutant-extrema-'):
        assert any('exact extremal endpoints differ' in str(f) for f in failures),'mutation escaped numeric endpoint gate'
        print('Expected endpoint-selection mutation detected numerically')
    else:
        assert not failures
        if reference is None:reference=result.stdout
        else:assert reference==result.stdout,'serialized baseline/candidate outputs differ'
