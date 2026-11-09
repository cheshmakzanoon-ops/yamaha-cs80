#!/usr/bin/env python3
"""Independent exact-rational elementary checks and high-precision diode/log checks.

No third-party package is required. Decimal transcendental values (120 digits)
are test oracles only; they are not part of the C++ certificate or its proof.
"""
from decimal import Decimal, localcontext
from fractions import Fraction
from pathlib import Path
import math, random, re, struct, subprocess, sys, os, hashlib

root=Path(__file__).resolve().parents[1]
executables=[Path(x).resolve() for x in sys.argv[1:]]
assert executables, 'give one or more compiled primitive-probe paths'
include_root=root/'src' if (root/'src/experimental/MemoryVerifiedDiode.h').exists() else root
header=Path(os.environ.get('CS80_VERIFIED_HEADER',include_root/'experimental/MemoryVerifiedDiode.h')).read_text()
match=re.search(r'logTwo\s*\{\s*([^,]+),\s*([^}]+)\}',header)
assert match, 'missing log-two proof constants'
ln_lo,ln_hi=(Fraction.from_float(float.fromhex(x.strip())) for x in match.groups())
z=Fraction(1,3); n=32
partial=2*sum((z**(2*j+1)/ (2*j+1) for j in range(n)),Fraction(0))
tail=2*z**(2*n+1)/((2*n+1)*(1-z*z))
assert ln_lo<=partial and partial+tail<=ln_hi, 'log-two rational enclosure proof FAILED'
print('Exact rational log(2) constant proof passed (32 terms + analytic tail)')

rng=random.Random(0x801555)
cases=[]
def hx(x): return x.hex() if math.isfinite(x) else str(x)
def add(kind,args,lo,hi,meta=None):
 cases.append((kind+' '+' '.join(hx(float(x)) for x in args),lo,hi,meta))
def finite_float():
 return math.ldexp(rng.uniform(.5,1.) * rng.choice([-1.,1.]),rng.randint(-500,500))
for kind in ('add','sub','mul','div'):
 for _ in range(350):
  a=sorted([finite_float(),finite_float()]); b=sorted([finite_float(),finite_float()])
  if kind=='div' and b[0]<=0<=b[1]: b=sorted([abs(b[0]),abs(b[1])])
  aa=list(map(Fraction.from_float,a)); bb=list(map(Fraction.from_float,b))
  fn={'add':lambda x,y:x+y,'sub':lambda x,y:x-y,'mul':lambda x,y:x*y,'div':lambda x,y:x/y}[kind]
  values=[fn(x,y) for x in aa for y in bb]
  add(kind,a+b,min(values),max(values))
# Exact point-operand paths: the generic interval oracle remains independent
# of which endpoint operations the C++ implementation eliminates.
saved_rng_state=rng.getstate()
for kind in ('add','sub','mul','div'):
 for mode in ('left','right','both'):
  for _ in range(150):
   a=sorted([finite_float(),finite_float()]); b=sorted([finite_float(),finite_float()])
   if mode in ('left','both'): a=[a[0],a[0]]
   if mode in ('right','both'): b=[b[0],b[0]]
   if kind=='div' and b[0]<=0<=b[1]: b=sorted([abs(b[0]),abs(b[1])])
   aa=list(map(Fraction.from_float,a)); bb=list(map(Fraction.from_float,b))
   fn={'add':lambda x,y:x+y,'sub':lambda x,y:x-y,'mul':lambda x,y:x*y,'div':lambda x,y:x/y}[kind]
   values=[fn(x,y) for x in aa for y in bb]
   add(kind,a+b,min(values),max(values))
rng.setstate(saved_rng_state)  # Preserve the pre-existing downstream random vectors.
# Deliberate gradual-underflow and near-cancellation cases.
tiny=float.fromhex('0x0.0000000000001p-1022'); normal=float.fromhex('0x1p-1022')
for kind,a,b in [('mul',tiny,tiny),('div',normal,3.),('add',1.,-math.nextafter(1.,0.)),('sub',1.,math.nextafter(1.,0.)),('mul',-tiny,tiny),('div',-normal,3.)]:
 A,B=Fraction.from_float(a),Fraction.from_float(b)
 truth={'mul':lambda:A*B,'div':lambda:A/B,'add':lambda:A+B,'sub':lambda:A-B}[kind]()
 add(kind,[a,a,b,b],truth,truth)
# Signed-zero points and ranges crossing zero, with explicit invalid quotient
# domains. Also compare overflow/nonfinite rejection paths in the raw outputs.
for zero in [0.,-0.]:
 for other in [[-2.,-1.],[-1.,1.],[1.,2.],[-tiny,tiny],[-0.,0.]]:
  for left in [True,False]:
   a,b=([zero,zero],other) if left else (other,[zero,zero])
   for kind in ('add','sub','mul','div'):
    if kind=='div' and b[0]<=0<=b[1]:
     add(kind,a+b,None,None,('required-rejection',)); continue
    aa=list(map(Fraction.from_float,a));bb=list(map(Fraction.from_float,b))
    fn={'add':lambda x,y:x+y,'sub':lambda x,y:x-y,'mul':lambda x,y:x*y,'div':lambda x,y:x/y}[kind]
    values=[fn(x,y) for x in aa for y in bb]
    add(kind,a+b,min(values),max(values))
maximum=float.fromhex('0x1.fffffffffffffp1023')
for kind,args in [('add',[maximum,maximum,maximum,maximum]),('sub',[maximum,maximum,-maximum,-maximum]),
                  ('mul',[maximum,maximum,2.,2.]),('div',[maximum,maximum,tiny,tiny])]:
 add(kind,args,None,None,('required-rejection',))
for kind in ('add','sub','mul','div'):
 for bad in [math.nan,math.inf,-math.inf]:
  add(kind,[bad,bad,0.,0.],None,None,('required-rejection',))
with localcontext() as ctx:
 ctx.prec=120
 values=[tiny,normal,math.nextafter(normal,0.),1.,math.nextafter(1.,0.),math.nextafter(1.,math.inf),2.,math.nextafter(2.,0.),float.fromhex('0x1.fffffffffffffp1023')]
 for k in [-1074,-1022,-1000,-500,-1,0,1,500,1000,1023]:
  x=math.ldexp(1.,k)
  values.extend(y for y in [x,math.nextafter(x,0.),math.nextafter(x,math.inf)] if y>0 and math.isfinite(y))
 # Both sides of the centered-series reduction threshold, including tiny and
 # largest-exponent inputs. These are explicit regressions, not random coverage.
 for base in [.75,1.5]:
  for exponent in [-1022,-500,0,500,1023]:
   x=math.ldexp(base,exponent)
   values.extend(y for y in [math.nextafter(x,0.),x,math.nextafter(x,math.inf)] if y>0 and math.isfinite(y))
 for _ in range(1700):
  bits=rng.randrange(1,0x7ff0000000000000)
  values.append(struct.unpack('>d',struct.pack('>Q',bits))[0])
 for x in values:
  y=Decimal.from_float(x).ln(); add('log',[x],y,y)

 def current(volts,r,s,a):
  V,R,S,A=map(Decimal.from_float,(volts,r,s,a))
  if V==0: return Decimal(0),Decimal(0)
  lo=Decimal(0) if V>0 else -S
  hi=V/R if V>0 else Decimal(0)
  # Bisection is independent of the C++ candidate and contractor. The lower
  # limiting endpoint is never logged. 150 steps are much finer than binary64.
  for _ in range(150):
   mid=(lo+hi)/2
   g=R*mid+A*((S+mid)/S).ln()-V
   if g>0: hi=mid
   else: lo=mid
  return lo,hi

 diode_count=0
 for r in [.5,.568,1.568,20.568,39000.,1e6]:
  for s,a in [(2.52e-9,1.752*.025852),(1e-8,1.6*.025852),(1e-9,2.*.025852)]:
   for volts in [-5.,-1.,-.1,-1e-9,0.,1e-9,.1,.7,1.,10.]:
    # Both a point and neighboring-double voltage interval. Monotonicity makes
    # endpoint roots the complete voltage-interval extrema.
    for uncertainty in [False,True]:
     vlo=math.nextafter(volts,-math.inf) if uncertainty and volts!=0 else volts
     vhi=math.nextafter(volts,math.inf) if uncertainty and volts!=0 else volts
     low=current(vlo,r,s,a)[0]; high=current(vhi,r,s,a)[1]
     hint=float((low+high)/2)
     for candidate in [hint,0.,math.nan,1e200]:
      add('diode',[vlo,vhi,r,s,a,candidate,1e-11],low,high,(r,s,a,volts,uncertainty))
      diode_count+=1
     if abs(volts)<=.7:
      for candidate in [hint,0.,math.nan,1e200]:
       add('diode',[vlo,vhi,r,s,a,candidate,1e-15],low,high,('optional-tight',r,s,a,volts,uncertainty))
       diode_count+=1

reference_raw=None
reference_status=None
for exe in executables:
 result=subprocess.run([str(exe)],input='\n'.join(c[0] for c in cases)+'\n',text=True,capture_output=True,check=True)
 if os.environ.get('CS80_COMPARE_RAW_INTERVALS')=='1':
  if reference_raw is None: reference_raw=result.stdout
  else: assert result.stdout==reference_raw, 'baseline/candidate raw interval outputs differ'
  print('Raw interval output SHA256:',hashlib.sha256(result.stdout.encode()).hexdigest())
 lines=result.stdout.splitlines()
 assert len(lines)==len(cases),(len(lines),len(cases),result.stderr)
 if os.environ.get('CS80_COMPARE_DIODE_AVAILABILITY')=='1':
  status=[line.split()[0] for line in lines]
  if reference_status is not None:
   changed=[(case[0],before,after) for case,before,after in zip(cases,reference_status,status) if before!=after]
   print('Baseline/candidate status changes:',len(changed))
   for change in changed: print('STATUS',change)
   if os.environ.get('CS80_REQUIRE_NO_NEW_REJECTIONS')=='1':
    assert not any(before=='1' and after!='1' for _,before,after in changed), 'candidate availability regressed'
  else: reference_status=status
 failed=[]; counts={}; worst_log_width=0.; worst_current_width=0.; tight_accepted=0; tight_rejected=0
 for case,line in zip(cases,lines):
  command,low,high,meta=case
  kind=command.split()[0]; counts[kind]=counts.get(kind,0)+1
  ok,l,h=line.split(); l=float.fromhex(l); h=float.fromhex(h)
  if meta is not None and meta[0]=='required-rejection':
   if ok!='0' or (math.isfinite(l) and math.isfinite(h) and l<=h):
    failed.append((command,line,'invalid elementary operation was not rejected'))
   continue
  if ok!='1' and meta is not None and meta[0]=='optional-tight':
   tight_rejected+=1
   if l!=-123. or h!=456.: failed.append((command,line,'tight rejection changed output'))
   continue
  if ok!='1' or not math.isfinite(l) or not math.isfinite(h) or l>h:
   failed.append((command,line,'invalid')); continue
  if meta is not None and meta[0]=='optional-tight': tight_accepted+=1
  if isinstance(low,Fraction):
   inside=Fraction.from_float(l)<=low and Fraction.from_float(h)>=high
  else:
   inside=Decimal.from_float(l)<=low and Decimal.from_float(h)>=high
  if not inside: failed.append((command,line,'oracle not enclosed',str(low),str(high)))
  if kind=='log': worst_log_width=max(worst_log_width,h-l)
  if kind=='diode':
   worst_current_width=max(worst_current_width,h-l)
   if h-l>float.fromhex(command.split()[-1]): failed.append((command,line,'requested width exceeded'))
 print('Executable:',exe)
 print('Independent vectors:',len(cases),counts)
 print('Largest log interval width:',worst_log_width,'; diode width A:',worst_current_width)
 print('Additional 1e-15 A availability probes: accepted',tight_accepted,'; safely rejected',tight_rejected)
 expected_failure=exe.name.startswith('mutant-')
 for failure in failed[:3 if expected_failure else 20]: print('FAIL',failure)
 print('Failures:',len(failed))
 if expected_failure:
  assert failed, 'fault injection escaped the independent vectors'
  print('Expected fault injection was detected')
 else:
  assert not failed
  subprocess.run([str(exe),'--self-tests'],check=True)
