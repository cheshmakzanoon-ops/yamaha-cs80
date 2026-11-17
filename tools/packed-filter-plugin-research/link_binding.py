"""Actual archived IgBatch member + live GNU-map + relocated ELF binding.

Pure byte/text validation. A separate recorded clean relink and hash-bound host
execution are mandatory; this module does not claim they occurred.
"""
from pathlib import Path
import re
from identity import require
from elf_proof import Elf, PROCESS, verify_process

MEMBER='Ig00156Batch.cpp.o'

def resolve(value,cwd):
    path=Path(value)
    path=path if path.is_absolute() else Path(cwd)/path
    require('..' not in path.parts,'noncanonical link owner')
    return str(path.absolute())

def contributions(text,cwd):
    marker='Linker script and memory map'
    require(text.count(marker)==1,'one live GNU map required')
    live=text.split(marker,1)[1].split('Cross Reference Table',1)[0]
    complete=re.compile(r'^\s+(\.\S+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+?)\s*$')
    continuation=re.compile(r'^\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+?)\s*$')
    pending=None;result=[]
    for line in live.splitlines():
        match=complete.match(line)
        if match:section,address,size,owner=match.groups();pending=None
        else:
            match=continuation.match(line) if pending else None
            if match:address,size,owner=match.groups();section=pending;pending=None
            else:
                value=line.strip();pending=value if line[:1].isspace() and re.fullmatch(r'\.\S+',value) else None
                continue
        archive=re.fullmatch(r'(.+)\(([^()]+)\)',owner)
        if archive:
            path,member=archive.groups();result.append(dict(section=section,address=int(address,16),size=int(size,16),archive=resolve(path,cwd),member=member))
        elif owner.endswith(MEMBER):
            result.append(dict(section=section,address=int(address,16),size=int(size,16),archive=None,member=resolve(owner,cwd)))
    return result

def verify_binding(*, object_bytes, member_bytes, member_names, archive_path, map_text, cwd, final_bytes):
    require(isinstance(member_names,list) and member_names.count(MEMBER)==1,'one filter archive member required')
    require(object_bytes==member_bytes and bool(object_bytes),'archive filter member differs from sealed object')
    result=verify_process(object_bytes,final_bytes)
    obj,final=Elf(object_bytes),Elf(final_bytes);osym,fsym=obj.symbol(PROCESS),final.symbol(PROCESS)
    records=[r for r in contributions(map_text,cwd) if r['member'].endswith(MEMBER) and r['size']>0]
    require({(r['archive'],r['member']) for r in records}=={(resolve(archive_path,cwd),MEMBER)},'missing/extra/direct filter map provider')
    sections=[r for r in records if r['section']==obj.sections[osym['section']]['name']]
    require(len(sections)==1,'one live filter process input section required')
    section=sections[0]
    require(osym['value']+osym['size']<=section['size'],'process outside mapped input section')
    require(fsym['value']==section['address']+osym['value'],'final process/map address mismatch')
    return {**result,'live_input_section':section,'archive_member_bound':True,
            'recorded_clean_relink_required':True,'host_execution_required':True,'production_qualified':False}
