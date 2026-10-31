#!/usr/bin/env python3
from pathlib import Path
import sys,json,datetime,hashlib
qualified = Path('/workspace/scratch/2f805bffd21a/cs80-ui-publication/scripts/check-state-tree-synchronization.py')
text = qualified.read_text()
marker = '\ntry:\n    with ThreadPoolExecutor(max_workers=args.jobs) as pool:'
assert text.count(marker) == 1
prefix = text.split(marker, 1)[0]
namespace = {'__file__': str(qualified), '__name__': '__main__'}
exec(compile(prefix, str(qualified), 'exec'), namespace)
report = namespace['report']; output = namespace['OUT']
report['scope'] = 'Focused integrated control at base3226011; prior separately-qualified mutations retained without rerun'
report['integration_runner_sha256'] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
try:
    with namespace['ThreadPoolExecutor'](max_workers=namespace['args'].jobs) as pool:
        list(pool.map(namespace['compile_one'], namespace['recipes'].items()))
    binary=output/'CS80Snapshots-integrated'
    report['control_link_command']=namespace['link_executable'](binary)
    for name,argument in [('state-tree-synchronization','--state-tree-test'),('host-automation-state-boundary','--host-automation-boundary-test')]:
        report['results'].append({'name':name,**namespace['run'](binary,output/name,[argument],0)})
        namespace['save']()
    report['status']='passed'
except BaseException as error:
    report['status']='failed'; report['error']=repr(error)
    raise
finally:
    report['finished_utc']=datetime.datetime.now(datetime.timezone.utc).isoformat()
    namespace['save']()
print(json.dumps({'status':report['status'],'results':[{'name':r['name'],'exit':r['exit']} for r in report['results']]},indent=2))
