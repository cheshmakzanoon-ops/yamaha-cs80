"""Fail-closed x86-64 ELF proof for the exact packed filter process function.

Reads bytes only. No execution, objcopy, disassembly heuristics or masked-target
acceptance. Supports the observed GNU13 PC32/PLT32 fixed-width relocations only.
Requires separate archive/map provenance and clean-link invocation evidence.
"""
import hashlib
import struct
from identity import require

PROCESS = '_ZN4cs8018Ig00156BatchAccess7processERKSt5arrayIPNS_12Ig00156StageELm4EERKS1_IfLm4EERS1_INS2_6OutputELm4EE'

def constant_access_width(code, relative, addend):
    a=code[max(0,relative-4):relative]
    b=code[max(0,relative-5):relative]
    if addend == -5:
        compare=(len(a)==4 and a[0]==0xc5 and a[1]&4 and a[2]==0xc2 and a[3]&0xc7==5)
        blend=(len(b)==5 and b[0]==0xc4 and b[1]&31==3 and b[2]&4 and b[3]==0x4b and b[4]&0xc7==5)
        require((compare or blend) and relative+5<=len(code), 'unproved trailing immediate relocation')
        return 32
    require(addend == -4, 'unsupported constant addend')
    if len(a)==4 and a[3]&0xc7==5:
        if a[0]==0xc5:
            pp=a[1]&3
            if a[2]==0x10 and pp in (2,3):return 4 if pp==2 else 8
            if a[2] in (0x28,0x58,0x59) and pp==1:return 32 if a[1]&4 else 16
        # Tiny offline proof fixture uses ordinary scalar SSE arithmetic.
        if a[0]==0xf2 and a[1]==0x0f and a[2] in (0x10,0x58,0x59):return 8
    if len(b)==5 and b[0]==0xc4 and b[1]&31==2 and b[2]&3==1 and b[4]&0xc7==5:
        if b[3]==0x19:return 8
        if b[3]==0x18:return 4
    raise ValueError('unproved RIP-relative constant access width')

class Elf:
    def __init__(self, data):
        self.data = data
        require(data[:7] == b'\x7fELF\x02\x01\x01' and len(data) >= 64, 'ELF64 little-endian required')
        header = struct.unpack_from('<HHIQQQIHHHHHH', data, 16)
        self.kind, machine = header[:2]
        require(machine == 62 and self.kind in (1, 2, 3), 'native x86-64 object/executable required')
        offset, entry, count, strings = header[5], header[10], header[11], header[12]
        require(entry == 64 and count > 0 and strings < count, 'ordinary ELF section table required')
        require(offset + entry*count <= len(data), 'truncated section table')
        self.sections = []
        for i in range(count):
            values = struct.unpack_from('<IIQQQQIIQQ', data, offset + entry*i)
            keys = ('nameoff','type','flags','addr','offset','size','link','info','align','entsize')
            self.sections.append(dict(zip(keys, values)))
        names = self.section_bytes(strings)
        for s in self.sections:
            s['name'] = self.string(names, s['nameoff'])
        self.tables = {}
        for i,s in enumerate(self.sections):
            if s['type'] not in (2,11):
                continue
            require(s['entsize'] == 24 and s['size'] % 24 == 0 and s['link'] < count, 'invalid symbol table')
            names = self.section_bytes(s['link']); rows=[]
            raw = self.section_bytes(i)
            for pos in range(0,len(raw),24):
                name,info,other,section,value,size = struct.unpack_from('<IBBHQQ',raw,pos)
                rows.append(dict(name=self.string(names,name),info=info,other=other,section=section,value=value,size=size))
            self.tables[i] = rows

    @staticmethod
    def string(data, offset):
        require(offset < len(data), 'invalid string offset')
        end = data.find(b'\0',offset);require(end >= 0, 'unterminated string')
        return data[offset:end].decode('utf-8')

    def section_bytes(self,index):
        require(0 <= index < len(self.sections), 'section out of bounds')
        s=self.sections[index];require(s['type'] != 8 and s['offset']+s['size'] <= len(self.data), 'section has no complete file bytes')
        return self.data[s['offset']:s['offset']+s['size']]

    def symbol(self,name):
        found=[s for t in self.tables.values() for s in t if s['name']==name and 0 < s['section'] < len(self.sections)]
        distinct={(s['section'],s['value'],s['size'],s['info']&15) for s in found}
        require(len(distinct)==1 and found[0]['size']>0 and found[0]['info']&15==2, 'one defined nonempty function required: '+name)
        return found[0]

    def symbol_bytes(self,symbol):
        sec=self.sections[symbol['section']]
        offset=symbol['value']-(sec['addr'] if self.kind!=1 else 0)
        require(sec['flags']&4 and not sec['flags']&1 and 0 <= offset and offset+symbol['size'] <= sec['size'], 'function outside executable section')
        return self.section_bytes(symbol['section'])[offset:offset+symbol['size']]

    def at_address(self,address,size):
        found=[i for i,s in enumerate(self.sections) if s['flags']&2 and not s['flags']&1 and s['type']!=8 and s['addr']<=address and address+size<=s['addr']+s['size']]
        require(len(found)==1, 'resolved relocation target not in one file section')
        s=self.sections[found[0]];offset=address-s['addr']
        return self.section_bytes(found[0])[offset:offset+size]


def verify_process(object_bytes, final_bytes):
    obj, final = Elf(object_bytes), Elf(final_bytes)
    require(obj.kind == 1 and final.kind in (2,3), 'object and linked ELF required')
    osym, fsym = obj.symbol(PROCESS), final.symbol(PROCESS)
    require(osym['size']==fsym['size'], 'process size differs')
    original=bytearray(obj.symbol_bytes(osym));linked=bytearray(final.symbol_bytes(fsym))
    relocations=[];covered=set(); proved_ranges=[(fsym['value'],fsym['size'])]
    for i,sec in enumerate(obj.sections):
        if sec['info'] != osym['section'] or sec['type'] not in (4,9):
            continue
        require(sec['type']==4 and sec['entsize']==24 and sec['size']%24==0 and sec['link'] in obj.tables, 'unsupported relocation section')
        raw=obj.section_bytes(i)
        for pos in range(0,len(raw),24):
            offset,info,addend=struct.unpack_from('<QQq',raw,pos)
            if not osym['value'] <= offset < osym['value']+osym['size']:
                continue
            index,kind=info>>32,info&0xffffffff
            require(kind in (2,4) and index < len(obj.tables[sec['link']]), 'unsupported function relocation')
            relative=offset-osym['value']
            require(relative+4<=len(original) and not covered.intersection(range(relative,relative+4)), 'invalid overlapping relocation')
            target=obj.tables[sec['link']][index]
            require(addend in (-4,-5), 'unsupported relocation addend')
            displacement=struct.unpack_from('<i',linked,relative)[0]
            resolved=fsym['value']+relative+displacement-addend
            if target['info']&15==2:
                require(addend == -4 and relative>0 and original[relative-1] in (0xe8,0xe9),
                        'unsupported helper call encoding')
                target_final=final.symbol(target['name'])
                for ri,rs in enumerate(obj.sections):
                    if rs['info']!=target['section'] or rs['type'] not in (4,9):
                        continue
                    stride=24 if rs['type']==4 else 16
                    require(rs['entsize']==stride and rs['size']%stride==0, 'invalid helper relocation table')
                    data=obj.section_bytes(ri)
                    for rp in range(0,len(data),stride):
                        roffset=struct.unpack_from('<Q',data,rp)[0]
                        require(not (roffset < target['value']+target['size'] and roffset+8 > target['value']),
                                'helper relocation requires separate proof')
                require(resolved==target_final['value'], 'function relocation target differs; PLT indirection needs separate proof')
                require(obj.symbol_bytes(target)==final.symbol_bytes(target_final),
                        'helper function bytes differ or require separate relocation proof')
                proved_ranges.append((target_final['value'],target_final['size']))
            else:
                require(0 < target['section'] < len(obj.sections), 'undefined constant relocation')
                source_section=obj.sections[target['section']]
                # Observed GCC fixed-size mergeable constant pools only; no guessed width.
                width=source_section['entsize']
                require(source_section['flags']&0x10 and not source_section['flags']&0x21
                        and width in (4,8,16,32),
                        'unsupported non-function relocation target')
                source=obj.section_bytes(target['section'])
                access=constant_access_width(original,relative,addend)
                require(target['value']%width+access<=width, 'constant operand not fully covered')
                base=target['value']//width*width
                require(base+width<=len(source), 'constant outside section')
                require(final.at_address(resolved-(target['value']-base),width)==source[base:base+width],
                        'resolved constant bytes differ')
                proved_ranges.append((resolved-(target['value']-base),width))
            relocations.append({'offset':relative,'kind':kind,'target':target['name'],'resolved':resolved})
            covered.update(range(relative,relative+4));original[relative:relative+4]=b'\0'*4;linked[relative:relative+4]=b'\0'*4
    require(original==linked, 'non-relocation process instruction bytes differ')
    require(bool(relocations), 'expected relocation-bearing function missing')
    # Never claim file-byte equality if the dynamic loader can rewrite those
    # proved bytes. Ordinary .rela.dyn/.rela.plt entries target data/GOT, not these
    # read-only code/constants. Reject any intersection without guessing a fixup.
    for i,sec in enumerate(final.sections):
        if sec['type'] not in (4,9):
            continue
        stride=24 if sec['type']==4 else 16
        require(sec['entsize']==stride and sec['size']%stride==0, 'invalid final relocation table')
        raw=final.section_bytes(i)
        for pos in range(0,len(raw),stride):
            address=struct.unpack_from('<Q',raw,pos)[0]
            require(not any(address < start+size and address+8 > start for start,size in proved_ranges),
                    'final relocation may rewrite proved bytes')
    return {'symbol':PROCESS,'function_size':len(original),'relocations_proved':len(relocations),
            'object_sha256':hashlib.sha256(object_bytes).hexdigest(),
            'final_sha256':hashlib.sha256(final_bytes).hexdigest(),
            'production_qualified':False}
