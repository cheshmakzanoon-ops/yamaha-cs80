import importlib.util
from pathlib import Path
import fcntl
import subprocess
import sys
root = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()
lock = Path(sys.argv[3]).resolve()
spec = importlib.util.spec_from_file_location('original_controls', root / 'tests/release/qualify_installer_binary_mutations.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
original_run = subprocess.run

def serialized_run(*args, **kwargs):
    with lock.open('a') as stream:
        fcntl.flock(stream, fcntl.LOCK_EX)
        return original_run(*args, **kwargs)

module.subprocess.run = serialized_run
sys.argv = [str(root / 'tests/release/qualify_installer_binary_mutations.py'), '--out', str(out)]
module.main()
