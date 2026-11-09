#!/usr/bin/env python3
"""Independent exact-Fraction oracle for centered mantissa/public logarithms."""
from fractions import Fraction as F
import math,random,subprocess,struct,sys
rng=random.Random(0x1280)
def ln_interval(m,n=32):
 z=(m-1)/(m+1); z2=z*z;p=z;s=F(0)
 for j in range(n): s+=2*p/(2*j+1);p*=z2
 tail=2*abs(p)/((2*n+1)*(1-z2))
 return (s,s+tail) if z>=0 else (s-tail,s)
ln2=ln_interval(F(2),64)
def oracle(x):
 f=F.from_float(x); k=f.numerator.bit_length()-f.denominator.bit_length()
 m=f/(F(2)**k)
 if m>F(3,2):m/=2;k+=1
 lo,hi=ln_interval(m)
 return lo+min(k*ln2[0],k*ln2[1]),hi+max(k*ln2[0],k*ln2[1])
cases=[]
for m in [.75,math.nextafter(.75,math.inf),math.nextafter(1.,0.),1.,math.nextafter(1.,math.inf),math.nextafter(1.5,0.),1.5]:
 cases.append(('m',m,ln_interval(F.from_float(m))))
for i in range(128):
 m=.75+.75*i/127
 cases.append(('m',m,ln_interval(F.from_float(m))))
for x in [float.fromhex('0x0.0000000000001p-1022'),float.fromhex('0x0.fffffffffffffp-1022'),float.fromhex('0x1.fffffffffffffp1023')]:cases.append(('x',x,oracle(x)))
for exponent in [-1022,-500,-1,0,1,500,1023]:
 for base in [.75,1.,1.5]:
  x=math.ldexp(base,exponent)
  for y in [math.nextafter(x,0.),x,math.nextafter(x,math.inf)]:
   if y>0 and math.isfinite(y):cases.append(('x',y,oracle(y)))
for i in range(256):
 bits=rng.randrange(1,0x7ff0000000000000);x=struct.unpack('>d',struct.pack('>Q',bits))[0]
 cases.append(('x',x,oracle(x)))
assert len(sys.argv)==2, 'give compiled MemoryVerifiedDiodeTests path'
exe=sys.argv[1]
r=subprocess.run([exe],input=''.join(f'{"mantissa" if op=="m" else "log"} {x.hex()}\n' for op,x,_ in cases),text=True,capture_output=True,check=True)
failed=[]
for case,line in zip(cases,r.stdout.splitlines()):
 op,x,(lo,hi)=case;good,ls,hs=line.split();a=float.fromhex(ls);b=float.fromhex(hs)
 if good!='1' or F.from_float(a)>lo or F.from_float(b)<hi:failed.append((case,line))
assert len(r.stdout.splitlines())==len(cases)
print('Independent exact-rational log enclosure checks:',len(cases),'failures:',len(failed))
for failure in failed[:3]:print(failure)
assert not failed
r=subprocess.run([exe],input=''.join(f'mantissa {x.hex()}\n' for x in [math.nextafter(.75,0.),math.nextafter(1.5,math.inf)]),text=True,capture_output=True,check=True)
assert all(line.startswith('0 ') for line in r.stdout.splitlines())
print('Both excluded mantissa-neighbor bounds rejected')
print('Exact worst series-tail upper bound at |z|=1/5:', float(2*F(1,5)**25/(25*(1-F(1,25)))))
