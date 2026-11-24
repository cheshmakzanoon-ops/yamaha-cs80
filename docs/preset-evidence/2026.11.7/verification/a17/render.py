#!/usr/bin/env python3
"""Focused factory Bell captures; temporary PCM only, no preset overrides."""
import concurrent.futures,pathlib,subprocess
ROOT=pathlib.Path(__file__).resolve().parents[5]
TMP=pathlib.Path('/tmp/cs80-a17-1cee5a')
NAMES=['Tubular Bells','Gong','Temple Bell','Agogo','Music Box']
def run(item):
 label,binary,note,hold,tail=item
 out=TMP/label
 cmd=[binary,'--out',str(out),'--wav','--note',str(note),'--velocity','.85','--hold',str(hold),'--tail',str(tail)]
 for name in NAMES: cmd+=['--preset',name]
 r=subprocess.run(cmd,text=True,capture_output=True)
 (TMP/(label+'.log')).write_text(r.stdout+r.stderr)
 if r.returncode: raise RuntimeError(label+r.stdout+r.stderr)
 print(label,r.stdout,flush=True)
if __name__=='__main__':
 jobs=[('current_n60','/tmp/cs80-build-1cee5a/cs80_preset_capture',60,8,2),('baseline_n60','/tmp/cs80-reference-1cee5a/cs80_preset_capture',60,8,2),('current_n48','/tmp/cs80-build-1cee5a/cs80_preset_capture',48,3,0),('current_n72','/tmp/cs80-build-1cee5a/cs80_preset_capture',72,3,0)]
 with concurrent.futures.ThreadPoolExecutor(max_workers=2) as pool: list(pool.map(run,jobs))
