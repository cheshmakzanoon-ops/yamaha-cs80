from pathlib import Path
import hashlib,json,subprocess
p=Path(__file__).resolve().parent;r=p.parent/'published-main';s=(r/'src/core/Ig00156Filter.h').read_text();assert s.count('namespace cs80\n{')==1
s=s.replace('namespace cs80\n{','namespace diagnostic\n{\nusing namespace cs80;\ninline int terminal=0;\ninline double residual=0;\ninline double coefficients[8]{};\ninline double trace[5][4]{};')
needle='            if (std::abs(f) < 1.0e-7 || iteration == 4)\n                break;'
assert s.count(needle)==1
s=s.replace(needle,'''            trace[iteration][0]=bp; trace[iteration][1]=hp; trace[iteration][2]=f;
            trace[iteration][3]=1.0 + (g1 * (1.0-t1*t1) + f1)*(c1 + g2*(1.0-t2*t2) + f2);
            if (std::abs(f) < 1.0e-7 || iteration == 4) {
                terminal=iteration; residual=f;
                coefficients[0]=zBp; coefficients[1]=c0; coefficients[2]=c1;
                coefficients[3]=g1; coefficients[4]=f1; coefficients[5]=level;
                coefficients[6]=u; coefficients[7]=inputScale;
                break;
            }''')
(p/'instrumented.h').write_text(s)
(p/'source.json').write_text(json.dumps({'commit':subprocess.check_output(['git','-C',str(r),'rev-parse','HEAD'],text=True).strip(),'header_sha256':hashlib.sha256((r/'src/core/Ig00156Filter.h').read_bytes()).hexdigest()},indent=2))
