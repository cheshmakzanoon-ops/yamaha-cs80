"""Lossless bounded CAS ZIP preparation. No upload, CLI, deletion or qualification.

Inputs/output parents must be quiescent and owned. Filesystem bytes, directory
inventory and POSIX modes are preserved; timestamps, owners, ACLs and xattrs are
NOT represented. Reject symlinks, hard links and special files rather than infer
ownership. Failed restoration leaves an explicit partial destination for audit.
"""
from pathlib import Path, PurePosixPath
import hashlib, json, os, re, shutil, stat, struct, tempfile, zipfile
from identity import require

CHUNK=1024*1024
CAP_KEYS={'raw_bytes','compressed_bytes','manifest_bytes','entries'}

def caps_check(caps):
    require(isinstance(caps,dict) and set(caps)==CAP_KEYS,'exact explicit caps required')
    require(all(type(v) is int and v>0 for v in caps.values()),'finite positive integer caps required')
    require(caps['compressed_bytes']<=2**31 and caps['manifest_bytes']<=64*1024*1024
            and caps['entries']<=60000,'unsupported envelope caps')

def digest_file(path):
    with Path(path).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()

def owned_path(value, *, existing=True):
    p=Path(value).absolute()
    require('..' not in p.parts,'parent traversal forbidden')
    require(not any(x.is_symlink() for x in (p,*p.parents)),'symlink path forbidden')
    if existing:require(p.exists(),'required path missing')
    return p

def path_key(value):
    require(isinstance(value,str) and value and len(value)<=4096 and '\\' not in value and '\0' not in value,'invalid path')
    p=PurePosixPath(value)
    require(not p.is_absolute() and p.as_posix()==value and '..' not in p.parts
            and all(len(x)<=255 for x in p.parts),'noncanonical path')
    return p

def arm_name(name):
    require(isinstance(name,str) and re.fullmatch('[A-Za-z0-9_-]{1,32}',name),'invalid arm name')

def same_bytes(a,b):
    with a.open('rb') as x,b.open('rb') as y:
        while True:
            p,q=x.read(CHUNK),y.read(CHUNK)
            if p!=q:return False
            if not p:return True

def inventory(root,caps):
    root=owned_path(root);require(root.is_dir(),'arm directory required')
    entries=[];sources={};raw=0
    def walk(p):
        nonlocal raw
        info=p.lstat();mode=stat.S_IMODE(info.st_mode)
        require(not mode & 0o7000,'special mode bits forbidden')
        key='.' if p==root else p.relative_to(root).as_posix();path_key(key)
        require(len(entries)<caps['entries'],'entry cap exceeded')
        if stat.S_ISDIR(info.st_mode):
            require(mode & 0o500==0o500,'directory owner read/search required')
            entries.append({'path':key,'type':'directory','mode':mode})
            for child in sorted(p.iterdir()):walk(child)
        else:
            require(stat.S_ISREG(info.st_mode) and info.st_nlink==1,'regular unlinked file required')
            require(mode & 0o400,'file owner read required')
            raw+=info.st_size;require(raw<=caps['raw_bytes'],'raw byte cap exceeded')
            sha=digest_file(p);require(p.stat().st_size==info.st_size,'file changed while hashing')
            entries.append({'path':key,'type':'file','mode':mode,'size':info.st_size,'sha256':sha})
            sources[key]=p
    walk(root)
    return entries,sources,raw

class BoundedFile:
    def __init__(self,file,limit):self.file=file;self.limit=limit
    def write(self,data):
        require(self.file.tell()+len(data)<=self.limit,'compressed byte cap exceeded')
        return self.file.write(data)
    def __getattr__(self,key):return getattr(self.file,key)

def strict_json(data):
    def pairs(items):
        result={}
        for k,v in items:
            require(k not in result,'duplicate JSON key');result[k]=v
        return result
    return json.loads(data,object_pairs_hook=pairs,parse_constant=lambda _:(_ for _ in ()).throw(ValueError('nonfinite JSON')))

def validate_manifest(m,caps):
    require(isinstance(m,dict) and set(m)=={'format','arms'} and m['format']=='cs80-evidence-cas-v1','unsupported manifest')
    require(isinstance(m['arms'],dict) and bool(m['arms']),'arms required')
    blobs={};raw=0;count=0
    for arm,entries in m['arms'].items():
        arm_name(arm);require(isinstance(entries,list) and entries,'nonempty arm inventory required')
        nodes={}
        for e in entries:
            require(isinstance(e,dict),'entry required');key=e.get('path');path_key(key)
            require(key not in nodes,'duplicate manifest path');nodes[key]=e
            kind=e.get('type');required={'path','type','mode'}|({'size','sha256'} if kind=='file' else set())
            require(kind in ('file','directory') and set(e)==required,'invalid entry fields')
            mode=e['mode'];require(type(mode) is int and 0<=mode<=0o777,'invalid mode')
            require(mode & (0o400 if kind=='file' else 0o500)==(0o400 if kind=='file' else 0o500),'unreadable manifest entry')
            count+=1;require(count<=caps['entries'],'entry cap exceeded')
            if kind=='file':
                size=e['size'];sha=e['sha256'];require(type(size) is int and size>=0,'invalid file size')
                require(isinstance(sha,str) and re.fullmatch('[0-9a-f]{64}',sha),'invalid blob digest')
                raw+=size;require(raw<=caps['raw_bytes'],'raw byte cap exceeded')
                require(sha not in blobs or blobs[sha]==size,'inconsistent blob size');blobs[sha]=size
        require('.' in nodes and nodes['.']['type']=='directory','arm root directory missing')
        for key,e in nodes.items():
            if key=='.':continue
            parent=PurePosixPath(key).parent.as_posix()
            require(parent in nodes and nodes[parent]['type']=='directory','missing/non-directory parent')
    return blobs,raw,count

def verify_archive(archive,*,caps):
    caps_check(caps);archive=owned_path(archive)
    require(archive.is_file() and archive.stat().st_size<=caps['compressed_bytes'],'compressed archive cap exceeded')
    # Bound central-directory allocation before ZipFile materializes its entries.
    with archive.open('rb') as f:
        require(archive.stat().st_size>=22,'truncated ZIP envelope');f.seek(-22,2)
        sig,disk,cd_disk,n_disk,n_total,size,offset,comment=struct.unpack('<4s4H2LH',f.read(22))
        require(sig==b'PK\x05\x06' and disk==cd_disk==comment==0 and n_disk==n_total
                and n_total<=caps['entries']+1 and n_total<65535,'unsupported ZIP envelope')
        require(size<=256*(caps['entries']+1) and offset+size==archive.stat().st_size-22,'central-directory bound/layout')
    with zipfile.ZipFile(archive) as z:
        infos=z.infolist();names=[i.filename for i in infos]
        require(len(infos)<=caps['entries']+1 and len(names)==len(set(names)),'duplicate/excess ZIP entries')
        for i in infos:
            require(not i.is_dir() and stat.S_IFMT(i.external_attr>>16)==stat.S_IFREG,'only regular ZIP entries allowed')
            require(not i.flag_bits & 1 and i.compress_type in (zipfile.ZIP_STORED,zipfile.ZIP_DEFLATED),'unsupported ZIP encoding')
        require('manifest.json' in names and z.getinfo('manifest.json').header_offset==0,'manifest missing/not first');info=z.getinfo('manifest.json')
        require(info.file_size<=caps['manifest_bytes'],'manifest byte cap exceeded')
        manifest=strict_json(z.read(info));blobs,raw,count=validate_manifest(manifest,caps)
        require(set(names)=={'manifest.json',*('blobs/'+h for h in blobs)},'missing/extra blob inventory')
        for sha,size in blobs.items():
            info=z.getinfo('blobs/'+sha);require(info.file_size==size,'blob length differs')
            h=hashlib.sha256();seen=0
            with z.open(info) as f:
                while block:=f.read(CHUNK):seen+=len(block);require(seen<=size,'expanded blob limit');h.update(block)
            require(seen==size and h.hexdigest()==sha,'blob bytes differ')
    return {'manifest':manifest,'logical_raw_bytes':raw,'unique_blob_bytes':sum(blobs.values()),'entries':count,
            'archive_bytes':archive.stat().st_size,'archive_sha256':digest_file(archive),'production_qualified':False}

def pack(arms,archive,*,caps):
    caps_check(caps);require(isinstance(arms,dict) and arms,'explicit arm roots required')
    roots={};manifest={'format':'cs80-evidence-cas-v1','arms':{}};blobs={};raw=0;count=0
    for arm,value in arms.items():
        arm_name(arm);root=owned_path(value)
        require(all(root!=p and not root.is_relative_to(p) and not p.is_relative_to(root) for p in roots.values()),'independent arm roots required')
        roots[arm]=root;entries,sources,n=inventory(root,caps);manifest['arms'][arm]=entries;raw+=n;count+=len(entries)
        require(raw<=caps['raw_bytes'] and count<=caps['entries'],'combined input cap exceeded')
        for e in entries:
            if e['type']!='file':continue
            sha=e['sha256'];source=sources[e['path']]
            if sha in blobs:require(same_bytes(blobs[sha],source),'digest collision/different dedup bytes')
            else:blobs[sha]=source
    encoded=json.dumps(manifest,sort_keys=True,separators=(',',':')).encode();require(len(encoded)<=caps['manifest_bytes'],'manifest byte cap exceeded')
    validate_manifest(manifest,caps)
    archive=owned_path(archive,existing=False);require(not archive.exists(),'archive destination exists')
    require(archive.parent.is_dir() and all(not archive.is_relative_to(p) for p in roots.values()),'archive outside arms required')
    fd,temp=tempfile.mkstemp(prefix='.evidence-pack-',dir=archive.parent);temp=Path(temp)
    def info(name):
        i=zipfile.ZipInfo(name);i.create_system=3;i.external_attr=(stat.S_IFREG|0o600)<<16;i.compress_type=zipfile.ZIP_DEFLATED;return i
    try:
        with os.fdopen(fd,'w+b') as f:
            with zipfile.ZipFile(BoundedFile(f,caps['compressed_bytes']),'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as z:
                z.writestr(info('manifest.json'),encoded)
                for sha,p in sorted(blobs.items()):
                    with p.open('rb') as src,z.open(info('blobs/'+sha),'w',force_zip64=True) as dst:shutil.copyfileobj(src,dst,CHUNK)
        for arm,root in roots.items():require(inventory(root,caps)[0]==manifest['arms'][arm],'source changed during packing')
        checked=verify_archive(temp,caps=caps);require(checked['manifest']==manifest,'packed manifest changed')
        os.link(temp,archive)  # Exclusive publication, never overwrite an existing file.
        return {**checked,'roundtrip_reconstruction_required':True}
    finally:temp.unlink(missing_ok=True)

def restore(archive,destination,*,caps):
    checked=verify_archive(archive,caps=caps);manifest=checked['manifest']
    root=owned_path(destination,existing=False);require(not root.exists(),'fresh restore destination required')
    require(root.parent.is_dir(),'restore parent missing');root.mkdir()
    with zipfile.ZipFile(archive) as z:
        for arm,entries in manifest['arms'].items():
            target=root/arm;target.mkdir()
            directories=[e for e in entries if e['type']=='directory']
            for e in sorted(directories,key=lambda e:len(PurePosixPath(e['path']).parts)):
                if e['path']!='.':(target/e['path']).mkdir()
            for e in entries:
                if e['type']!='file':continue
                p=target/e['path']
                with p.open('xb') as dst,z.open('blobs/'+e['sha256']) as src:shutil.copyfileobj(src,dst,CHUNK)
                require(p.stat().st_size==e['size'] and digest_file(p)==e['sha256'],'restored blob differs');p.chmod(e['mode'])
            for e in sorted(directories,key=lambda e:len(PurePosixPath(e['path']).parts),reverse=True):(target/e['path']).chmod(e['mode'])
            observed=inventory(target,caps)[0]
            require(sorted(observed,key=lambda e:e['path'])==sorted(entries,key=lambda e:e['path']),'roundtrip inventory/modes differ')
    require(digest_file(archive)==checked['archive_sha256'],'archive changed during restore')
    return {**checked,'roundtrip_reconstruction_required':False,'restored':str(root)}
