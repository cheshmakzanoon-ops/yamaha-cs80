#!/usr/bin/env python3
"""Exact-Fraction endpoint oracle for the offline log-interval span bound.

No Decimal, libm logarithm, implementation interval helper or sampled interior
point supplies an acceptance oracle. Monotonicity makes the two exact endpoint
logs sufficient. Their rational atanh enclosures include a proved series tail.
"""
from fractions import Fraction as F
from pathlib import Path
import math, random, struct, subprocess, sys

def log_bounds(x):
    exact = F.from_float(x)
    k = exact.numerator.bit_length() - exact.denominator.bit_length()
    m = exact / F(2)**k
    if m > F(3, 2):
        m /= 2
        k += 1
    def series(y, terms):
        z = (y-1)/(y+1)
        z2 = z*z
        power, partial = z, F(0)
        for j in range(terms):
            partial += 2*power/(2*j+1)
            power *= z2
        tail = 2*abs(power)/((2*terms+1)*(1-z2))
        return (partial, partial+tail) if z >= 0 else (partial-tail, partial)
    a, b = series(m, 40)
    return a+min(k*LN2[0], k*LN2[1]), b+max(k*LN2[0], k*LN2[1])

# Separate positive rational log(2) series, not a stored C++ or libm constant.
z = F(1, 3)
partial = sum((2*z**(2*j+1)/(2*j+1) for j in range(80)), F(0))
LN2 = partial, partial+2*z**161/(161*(1-z*z))
rng = random.Random(0x801555940)
maximum = float.fromhex('0x1.fffffffffffffp1023')
tiny = float.fromhex('0x0.0000000000001p-1022')
normal = float.fromhex('0x1p-1022')
cases = []
def add(lo, hi, label):
    assert 0 < lo <= hi and math.isfinite(hi)
    cases.append((lo, hi, label, log_bounds(lo)[0], log_bounds(hi)[1]))

def nearby(lo, steps):
    bits = struct.unpack('>Q', struct.pack('>d', lo))[0]
    return struct.unpack('>d', struct.pack('>Q', min(bits+steps,0x7fefffffffffffff)))[0]

# Boundary neighborhoods include both atanh reduction sides, log sign changes,
# zero-width intervals, gradual underflow and largest finite endpoints.
anchors = [tiny, nearby(tiny,1), math.nextafter(normal,0.), normal, maximum]
for exponent in [-1022,-500,-1,0,1,500,1023]:
    for base in [.75,1.,1.5]:
        value = math.ldexp(base,exponent)
        anchors.extend(x for x in [math.nextafter(value,0.),value,math.nextafter(value,math.inf)]
                       if 0 < x <= maximum)
for lo in anchors:
    for steps in [0,1,2,8,128,2048,4095,4096,4097,8192]:
        add(lo,nearby(lo,steps),'boundary')

# Immediately below/at/above the derivative-branch threshold. At lo=1 the
# missing correction cannot be concealed by a wide log(a) enclosure.
for lo in [1., math.nextafter(1.,0.), .75,1.5,2.,normal,math.ldexp(1.,500)]:
    hi = lo*(1.+2.**-40)
    for edge in [math.nextafter(hi,lo),hi,math.nextafter(hi,math.inf)]:
        add(lo,edge,'threshold')
for lo,hi in [(tiny,maximum),(tiny,normal),(.5,2.),(.75,1.5),
              (math.nextafter(1.,0.),math.nextafter(1.,math.inf)),
              (1.,nearby(1.,4095)),(1.,nearby(1.,4094))]:
    add(lo,hi,'explicit')

for index in range(384):
    bits = rng.randrange(1,0x7ff0000000000000)
    lo = struct.unpack('>d',struct.pack('>Q',bits))[0]
    add(lo,nearby(lo,rng.choice([0,1,2,3,7,64,256,2048,4095,8192])),'random-narrow')
    other = struct.unpack('>d',struct.pack('>Q',rng.randrange(1,0x7ff0000000000000)))[0]
    add(min(lo,other),max(lo,other),'random-wide')
invalid = [(0.,1.),(-tiny,1.),(-1.,-tiny),(1.,0.),(2.,1.),
           (math.nan,1.),(1.,math.nan),(1.,math.inf),(-math.inf,1.)]
commands = ''.join(f'log-interval {lo.hex()} {hi.hex()}\n' for lo,hi,*_ in cases)
commands += ''.join(f'log-interval {lo.hex()} {hi.hex()}\n' for lo,hi in invalid)
assert len(sys.argv)>1, 'give compiled interval probe(s)'
reference = None
for name in sys.argv[1:]:
    exe = Path(name).resolve()
    result = subprocess.run([str(exe)],input=commands,text=True,capture_output=True,check=True)
    lines = result.stdout.splitlines()
    assert len(lines)==len(cases)+len(invalid),(len(lines),len(cases),result.stderr)
    failures = []
    for case,line in zip(cases,lines):
        lo,hi,label,truth_lo,truth_hi = case
        good,left,right = line.split()
        a,b = float.fromhex(left),float.fromhex(right)
        if good!='1' or not math.isfinite(a) or not math.isfinite(b) or a>b:
            failures.append((lo.hex(),hi.hex(),label,line,'invalid')); continue
        if F.from_float(a)>truth_lo or F.from_float(b)<truth_hi:
            failures.append((lo.hex(),hi.hex(),label,line,'endpoint log not enclosed'))
    for line in lines[len(cases):]:
        good,left,right=line.split()
        if good!='0': failures.append(('invalid domain accepted',line))
    if reference is not None:
        unchanged_wide = changed_wide = unchanged_point = 0
        for case,before,after in zip(cases,reference,lines):
            lo,hi,*_=case
            if hi==lo:
                unchanged_point += before==after
                if before!=after: failures.append(('point differs from baseline',lo.hex()))
            if F.from_float(hi)-F.from_float(lo) > F.from_float(lo)*F(1,2**39):
                unchanged_wide += before==after
                changed_wide += before!=after
        print('Unchanged point/wide:',unchanged_point,unchanged_wide,'changed wide:',changed_wide)
        if changed_wide: failures.append(('wide fallback differs from baseline',changed_wide))
    else:
        reference=lines
    print('Executable:',exe)
    print('Exact-rational log-interval cases:',len(cases),'invalid domains:',len(invalid),'failures:',len(failures))
    for failure in failures[:5]: print('FAIL',failure)
    # Mutation runs must fail the numeric oracle, not merely exit or compile.
    if exe.name.startswith('mutant-span-'):
        assert any('endpoint log not enclosed' in str(f) for f in failures), 'span mutation escaped numeric containment gate'
        print('Expected span mutation detected numerically')
    else:
        assert not failures
