"""Pure prospective schedule and decision. No rendering or timing."""
import math

def schedule():
    result=[]
    for phase,other in [('aa','stock'),('candidate','candidate')]:
        for round in range(3):
            for row in (range(7) if round!=1 else reversed(range(7))):
                for role in (('A','B') if round!=1 else ('B','A')):
                    result.append(dict(index=len(result),phase=phase,round=round,row=row,role=role,arm='stock' if role=='A' else other))
    return result

def gm(xs):return math.exp(sum(map(math.log,xs))/len(xs))
def decision(aa,candidate=None):
    for matrix in [aa]+([] if candidate is None else [candidate]):
        if len(matrix)!=3 or any(len(row)!=7 for row in matrix):raise ValueError('exact 3x7 ratios required')
        if any(not math.isfinite(x) or x<=0 for row in matrix for x in row):raise ValueError('finite positive ratios required')
    ap=[gm(r[:4]) for r in aa];noise=max(abs(x-1) for x in ap)
    if any(not .97<=x<=1.03 for r in aa for x in r) or any(not .985<=x<=1.015 for x in ap):return dict(decision='STOP_NO_RETRY_AA',noise=noise)
    if candidate is None:return dict(decision='AA_PASS_ONLY',noise=noise)
    cp=[gm(r[:4]) for r in candidate];gain=1-gm(cp);minimum=max(.005,2*noise)
    if any(not .97<=r[6]<=1.03 for r in candidate):return dict(decision='STOP_NO_RETRY_LEGACY_CONTROL',noise=noise,primary_gain=gain,primary_ratios=cp)
    useful=all(x<1 for x in cp) and gain>minimum
    return dict(decision='MEASURED_USEFUL_GAIN' if useful else 'NO_RESOLVED_USEFUL_GAIN',noise=noise,primary_gain=gain,primary_ratios=cp,minimum_gain=minimum,broad_lower_cost_statement_allowed=useful and all(x<=1.03 for r in candidate for x in r))
