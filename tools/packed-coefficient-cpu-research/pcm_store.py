"""Canonical exact-PCM receipts, used only after each completed run, outside timing."""
import hashlib,json,os,stat
from pathlib import Path
PCM_BYTES=23040000
MAX_UNIQUE=7

def require(ok,why):
    if not ok: raise ValueError(why)

def safe(path):
    path=Path(path)
    for p in (path,*path.parents):
        require(not p.is_symlink(),'symlink forbidden')
    return path

def regular(path):
    p=safe(path);s=p.stat();require(stat.S_ISREG(s.st_mode) and s.st_nlink==1,'regular single-link required');return p

def canonicalize(run,root,*,expected_bytes=PCM_BYTES):
    """Quiescent owned roots required. Never removes data until equivalent retained bytes verified."""
    root=safe(Path(root).absolute());run=safe(Path(run).absolute())
    require(run.is_relative_to(root) and run!=root,'run must be within owned root')
    raw=regular(run/'audio.f32');require(raw.stat().st_size==expected_bytes,'PCM bytecount')
    data=raw.read_bytes();digest=hashlib.sha256(data).hexdigest();store=root/'canonical-pcm';safe(store);store.mkdir(exist_ok=True)
    canonical=store/(digest+'.f32');reference=run/'audio-reference.json';require(not reference.exists(),'reference already exists')
    if canonical.exists():
        require(regular(canonical).read_bytes()==data,'hash-name collision or corrupted canonical')
    else:
        existing=list(store.iterdir());require(all(regular(p).suffix=='.f32' for p in existing),'unexpected store content')
        require(len(existing)<MAX_UNIQUE,'unique PCM limit exceeded; raw output retained')
        # Exclusive creation, no overwriting or pruning. Partial write remains for failure audit.
        with canonical.open('xb') as f:f.write(data);f.flush();os.fsync(f.fileno())
        require(regular(canonical).read_bytes()==data,'retained copy mismatch')
    receipt={'schema':1,'relative_path':str(canonical.relative_to(root)),'bytes':len(data),'sha256':digest}
    with reference.open('x') as f:json.dump(receipt,f,indent=2);f.write('\n');f.flush();os.fsync(f.fileno())
    # Final equivalent-copy verification immediately before removing only this owned raw duplicate.
    require(regular(raw).read_bytes()==regular(canonical).read_bytes()==data,'PCM changed before dedup')
    raw.unlink()
    return receipt

def read_pcm(run,root=None):
    run=safe(Path(run).absolute());raw=run/'audio.f32';ref=run/'audio-reference.json'
    if not ref.exists():return regular(raw).read_bytes()
    require(root is not None,'canonical receipt needs explicit owned root');root=safe(Path(root).absolute());require(run.is_relative_to(root),'run outside root')
    receipt=json.loads(regular(ref).read_text());digest=receipt['sha256'];require(len(digest)==64 and all(c in '0123456789abcdef' for c in digest),'invalid digest')
    require(receipt['relative_path']=='canonical-pcm/'+digest+'.f32','exact canonical relative path required')
    data=regular(root/receipt['relative_path']).read_bytes();require(len(data)==receipt['bytes'] and hashlib.sha256(data).hexdigest()==digest,'canonical receipt mismatch')
    if raw.exists():require(regular(raw).read_bytes()==data,'retained duplicate mismatch')
    return data
