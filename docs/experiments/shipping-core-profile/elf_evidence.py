"""Read-only ELF64 helpers. No module loading, compilation, or execution."""
import bisect, hashlib, json, re, struct, subprocess
from pathlib import Path

UNKNOWN = '[main ELF outside known function bounds]'

def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':')).encode()).hexdigest()

def source_name(text):
    if text == '??:0' or text == '??:?':
        return text
    if '/src/' in text:
        return 'project/' + text.rsplit('/src/', 1)[1]
    if '/usr/include/c++/14/' in text:
        return 'toolchain/libstdc++-14/' + text.split('/usr/include/c++/14/', 1)[1]
    if '/usr/lib/gcc/x86_64-linux-gnu/14/include/' in text:
        return 'toolchain/gcc-14/include/' + text.split('/usr/lib/gcc/x86_64-linux-gnu/14/include/', 1)[1]
    raise ValueError('unexpected source-location category')

class Elf:
    def __init__(self, path):
        self.bytes = Path(path).read_bytes()
        b = self.bytes
        assert b[:6] == b'\x7fELF\x02\x01', 'ELF64 little-endian required'
        h = struct.unpack_from('<16sHHIQQQIHHHHHH', b)
        assert h[2] == 62, 'x86-64 ELF required'
        self.type = h[1]
        self.sections = [struct.unpack_from('<IIQQQQIIQQ', b, h[6]+i*h[11]) for i in range(h[12])]
        strings = self.data(self.sections[h[13]])
        self.names = [self.string(strings, s[0]) for s in self.sections]
        self.symbols = {}
        for index, s in enumerate(self.sections):
            if s[1] not in (2, 11):
                continue
            strings = self.data(self.sections[s[6]])
            rows = []
            for off in range(s[4], s[4]+s[5], s[9]):
                name, info, other, ndx, val, size = struct.unpack_from('<IBBHQQ', b, off)
                rows.append((self.string(strings, name), info, other,
                             self.names[ndx] if 0 < ndx < len(self.names) else ndx, val, size))
            self.symbols[index] = rows

    @staticmethod
    def string(table, off):
        return table[off:table.find(b'\0', off)].decode()

    def data(self, s):
        return b'\0'*s[5] if s[1] == 8 else self.bytes[s[4]:s[4]+s[5]]

    @property
    def sha256(self):
        return hashlib.sha256(self.bytes).hexdigest()

    def allocatable(self):
        return {name: {'type':s[1], 'flags':s[2], 'addr':s[3], 'size':s[5],
                       'alignment':s[8], 'sha256':hashlib.sha256(self.data(s)).hexdigest()}
                for name,s in zip(self.names,self.sections) if s[2]&2}

    def relocations(self):
        out = {}
        for s in self.sections:
            if s[1] != 4 or not self.sections[s[7]][2]&2:
                continue
            rows = []
            for off in range(s[4], s[4]+s[5], s[9]):
                addr, info, add = struct.unpack_from('<QQq', self.bytes, off)
                rows.append((addr, info&0xffffffff, add, self.symbols[s[6]][info>>32]))
            out[self.names[s[7]]] = rows
        return out

    def executable_ranges(self):
        return [[name, hex(s[3]), hex(s[3]+s[5])]
                for name,s in zip(self.names,self.sections) if s[2]&6 == 6 and s[5]]

    def physical_map(self):
        functions = {}
        for i, table in self.symbols.items():
            if self.sections[i][1] != 2:
                continue
            for name, info, other, ndx, val, size in table:
                if info&15 == 2 and size and ndx:
                    functions.setdefault((val,size), []).append(name)
        keys = sorted(functions)
        raw = [sorted(functions[k])[0] for k in keys]
        demangled = subprocess.check_output(['c++filt'], input='\n'.join(raw)+'\n', text=True).splitlines()
        names = dict(zip(keys, demangled)); starts = [k[0] for k in keys]
        def lookup(addr):
            i = bisect.bisect_right(starts, addr)-1
            if i >= 0 and addr < sum(keys[i]):
                return names[keys[i]]
            return UNKNOWN
        return lookup, names

def dwarf(path, addresses):
    output = subprocess.check_output(['addr2line', '-e', str(path), '-a', '-f', '-C', '-i'],
        input='\n'.join(hex(a) for a in sorted(addresses))+'\n', text=True)
    result = {}; current = None; lines = []
    def finish():
        if current is not None:
            assert len(lines)%2 == 0
            result[current] = [[lines[i], source_name(lines[i+1])] for i in range(0,len(lines),2)]
    for line in output.splitlines():
        if line.startswith('0x'):
            finish(); current = int(line,16); lines = []
        else:
            lines.append(line)
    finish()
    return result
