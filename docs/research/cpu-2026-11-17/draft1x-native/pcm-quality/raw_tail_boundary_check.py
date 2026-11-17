"""Read-only followup: distinguish nominal-alignment next-onset overlap from raw late tail."""
import json,numpy as np,sys
from pathlib import Path
root=Path(sys.argv[1]);out=Path(sys.argv[2]);rows=[]
for arm in ['stock','draft']:
 p=next(p for p in sorted(root.glob('run-*')) if json.loads((p/'result.json').read_text())['arm']==arm and json.loads((p/'result.json').read_text())['row']==5)
 ref=json.loads((p/'audio-reference.json').read_text());a=np.fromfile(root/ref['relative_path'],dtype='<f4').reshape(-1,2).astype(float)
 vals=[]
 for i in range(10):
  lo=i*288000+240000;hi=(i+1)*288000;x=a[lo:hi]
  vals.append({'cycle':i,'rms':np.sqrt(np.mean(x*x,axis=0)).tolist(),'peak':np.max(np.abs(x),axis=0).tolist()})
 rows.append({'arm':arm,'raw_late_release_windows':vals})
(out/'raw-tail-boundary-check.json').write_text(json.dumps(rows,indent=2)+'\n')
