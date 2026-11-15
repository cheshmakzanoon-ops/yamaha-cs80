from pathlib import Path
import math,json
p=Path(__file__).resolve().parent
# Same rational law and strict clamps as production. No mathematical tanh substitution.
def T(x):
 if x>4.97:return 1.
 if x< -4.97:return -1.
 y=x*x
 return x*(135135.+y*(17325.+y*(378.+y)))/(135135.+y*(62370.+y*(3150.+y*28.)))
rows=[]
for line in (p/'run.log').read_text().splitlines():
 if not line.startswith('CAP,'):continue
 r=line.split(',');assert len(r)==50
 v=[float.fromhex(x)for x in r[6:]];z,c0,c1,g,ff,L,u,scale=v[16:24];recorded=v[42];bp=v[40];inv=1/L
 evaluations=0
 def F(x):
  global evaluations
  evaluations+=1
  h=c0+c1*x+g*L*T(x*inv)+ff*x
  return x-z+g*L*T(h*inv)+ff*h
 observed=F(bp);assert math.isclose(observed,recorded,rel_tol=1e-13,abs_tol=1e-12)
 A=1+ff*(c1+ff);B=ff*c0-z;C=g*L*(1+ff)
 prereq=all(math.isfinite(x)for x in (A,B,C,inv))and A>0 and c1>=0 and g>=0 and ff>=0 and L>0
 success=False;reason='prerequisite';best=bp;bestf=observed;expands=0
 if prereq:
  lo=(-B-C)/A;hi=(-B+C)/A;fl=F(lo);fh=F(hi)
  for _ in range(8):
   if fl<=0<=fh:break
   width=max(hi-lo,1.);lo-=width;hi+=width;fl=F(lo);fh=F(hi);expands+=1
  reason='no_verified_sign_bracket'
  if fl<=0<=fh:
   reason='iteration_limit'
   for x,fx in [(lo,fl),(hi,fh)]:
    if abs(fx)<abs(bestf):best,bestf=x,fx
   if abs(bestf)<1e-7:success=True;reason='endpoint_residual'
   for _ in range(96):
    if success:break
    mid=lo+(hi-lo)*.5
    if mid==lo or mid==hi:reason='midpoint_stagnation';break
    fm=F(mid)
    if not math.isfinite(fm):reason='nonfinite';break
    if abs(fm)<abs(bestf):best,bestf=mid,fm
    if abs(fm)<1e-7:success=True;reason='residual';break
    if fm<0:lo=mid
    else:hi=mid
 rows.append(dict(case=int(r[1]),clock=int(r[2]),lp=int(r[3]),pattern=int(r[4]),sample=int(r[5]),accepted=success,reason=reason,prerequisites=prereq,original_residual=recorded,root=best,root_residual=bestf,displacement=best-bp,evaluations=evaluations,expansions=expands))
assert len(rows)==768
summary={str(c):dict(captures=sum(x['clock']==c for x in rows),accepted=sum(x['clock']==c and x['accepted']for x in rows),max_abs_root_residual=max(abs(x['root_residual'])for x in rows if x['clock']==c),max_abs_displacement=max(abs(x['displacement'])for x in rows if x['clock']==c))for c in range(3)}
summary['all_prerequisites']=all(x['prerequisites']for x in rows);summary['max_equation_evaluations_including_original_check']=max(x['evaluations']for x in rows);summary['max_expansions']=max(x['expansions']for x in rows)
(p/'roots.json').write_text(json.dumps(rows,indent=2));(p/'root-summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2));print('FAILURES',json.dumps([x for x in rows if not x['accepted']]))
