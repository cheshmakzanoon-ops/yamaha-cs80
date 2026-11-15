from fractions import Fraction as F
from math import comb
import json
from pathlib import Path
# T=N/D; derivative numerator N'D-ND' in ascending powers of y=x^2.
a=[18261468225,-1404728325,58939650,-1819125,47250,-1134,28]
limit=F(497,100); L=limit*limit
bernstein=[sum(F(a[i])*L**i*F(comb(k,i),comb(6,i)) for i in range(k+1)) for k in range(7)]
assert all(x>0 for x in bernstein)
y=L;edge=limit*(135135+y*(17325+y*(378+y)))/(135135+y*(62370+y*(3150+y*28)))
assert 0<edge<1
result={'interval':'x in [-4.97,4.97]','derivative_numerator_coefficients_ascending_in_x_squared':a,'exact_positive_Bernstein_coefficients':[str(x)for x in bernstein],'positive_denominator':'135135+62370*x^2+3150*x^4+28*x^6','edge_value':str(edge),'upward_clamp_jump':float(1-edge),'conclusion':'Exact-real rational branch strictly increasing; outer constant branches and upward jumps make T nondecreasing and bounded [-1,1]. Not a floating-rounding monotonicity proof.'}
p=Path(__file__).resolve().parent;(p/'monotonic.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
