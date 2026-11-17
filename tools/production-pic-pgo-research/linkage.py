"""Read-only GNU ELF/archive/map evidence for the exact exercised VST3.

Not a runtime proof on its own: the orchestrator must hash-bind the output to the
recorded link invocation and unchanged host-test paths. No DSP/plugin execution.
"""
import hashlib
from pathlib import Path
import re
import subprocess

RENDER = '_ZN4cs806Engine6renderEPfiPKf'
MEMBER = 'Engine.cpp.o'


def require(ok, message):
    if not ok:
        raise ValueError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def resolve(path, cwd):
    path = Path(path)
    return str((path if path.is_absolute() else Path(cwd) / path).resolve())


def symbol_record(text, name):
    found = []
    for line in text.splitlines():
        parts = line.split()
        if len(parts) >= 6 and parts[-1] == name:
            require(parts.count('F') == 1, 'render must be a function')
            type_index = parts.index('F')
            require(type_index in (1, 2), 'unexpected symbol flags')
            require(len(parts) >= type_index + 4, 'incomplete function record')
            section, size = parts[type_index + 1:type_index + 3]
            visibility = parts[type_index + 3:-1]
            require(visibility in ([], ['.hidden'], ['.protected'], ['.internal']),
                    'unsupported symbol visibility record')
            require(section not in ('*UND*', '*ABS*'), 'symbol must have real defined section')
            found.append({'address': int(parts[0], 16), 'size': int(size, 16),
                          'section': section})
    require(len(found) == 1 and found[0]['size'] > 0, 'exactly one nonempty defined render symbol')
    return found[0]


def live_contributions(text, cwd):
    marker = 'Linker script and memory map'
    require(text.count(marker) == 1, 'GNU live-map marker missing/duplicate')
    text = text.split(marker, 1)[1].split('Cross Reference Table', 1)[0]
    records = []
    pending = None
    # GNU ld prints long input section names on a separate indented line.
    full = re.compile(r'^\s+(\.\S+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+?)\s*$')
    continuation = re.compile(r'^\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)\s+(.+?)\s*$')
    for line in text.splitlines():
        match = full.match(line)
        if match:
            section, address, size, owner = match.groups(); pending = None
        else:
            match = continuation.match(line) if pending else None
            if match:
                address, size, owner = match.groups(); section = pending; pending = None
            else:
                stripped = line.strip()
                pending = stripped if line[:1].isspace() and re.fullmatch(r'\.\S+', stripped) else None
                continue
        archive = re.fullmatch(r'(.+)\(([^()]+)\)', owner)
        if archive:
            archive_path, member = archive.groups()
            records.append({'section': section, 'address': int(address, 16), 'size': int(size, 16),
                            'archive': resolve(archive_path, cwd), 'member': member})
        elif owner.endswith('Engine.cpp.o'):
            records.append({'section': section, 'address': int(address, 16), 'size': int(size, 16),
                            'archive': None, 'member': resolve(owner, cwd)})
    return records


def verify_linkage(*, engine_bytes, archive_member_bytes, archive_members, archive_path,
                   object_symbols, final_symbols, map_text, cwd, final_bytes):
    require(engine_bytes and final_bytes, 'actual object and final binary bytes required')
    require(archive_members.count(MEMBER) == 1, 'one Engine archive member required')
    require(engine_bytes == archive_member_bytes, 'archive Engine bytes differ from frozen candidate')
    obj = symbol_record(object_symbols, RENDER)
    final = symbol_record(final_symbols, RENDER)
    require(obj['size'] == final['size'], 'resolved render size changed')
    records = live_contributions(map_text, cwd)
    engine_records = [row for row in records if row['member'].endswith(MEMBER) and row['size'] > 0]
    providers = {(row['archive'], row['member']) for row in engine_records}
    expected = (resolve(archive_path, cwd), MEMBER)
    require(providers == {expected}, 'missing, direct or duplicate Engine provider in live map')
    sections = [row for row in engine_records if row['section'] == obj['section']]
    require(len(sections) == 1, 'one live Engine input section required')
    section = sections[0]
    require(obj['address'] + obj['size'] <= section['size'], 'render extent outside member section')
    require(final['address'] == section['address'] + obj['address'], 'stale/wrong resolved render address')
    require(final['section'].startswith('.text'), 'render did not resolve into final executable text')
    require('__gcov' not in final_symbols and '__llvm_profile' not in final_symbols,
            'instrumentation symbol present in final binary')
    return {'engine_sha256': digest(engine_bytes), 'archive_member_sha256': digest(archive_member_bytes),
            'final_binary_sha256': digest(final_bytes), 'render_symbol': RENDER,
            'object_render': obj, 'final_render': final, 'live_input_section': section,
            'scope': 'archive member and resolved live render linkage; host execution separately hash-bound'}


def inspect(engine, archive, final, map_path, cwd):
    """Gather raw read-only tool outputs; caller retains them and final binary bytes."""
    def command(args):
        return subprocess.check_output(args, cwd=cwd)
    members = command(['ar', 't', str(archive)]).decode().splitlines()
    member = command(['ar', 'p', str(archive), MEMBER])
    object_symbols = command(['objdump', '-t', str(engine)]).decode()
    final_symbols = command(['objdump', '-t', str(final)]).decode()
    map_text = Path(map_path).read_text()
    report = verify_linkage(engine_bytes=Path(engine).read_bytes(), archive_member_bytes=member,
        archive_members=members, archive_path=str(archive), object_symbols=object_symbols,
        final_symbols=final_symbols, map_text=map_text, cwd=str(cwd), final_bytes=Path(final).read_bytes())
    return report, {'archive_members': members, 'object_symbols': object_symbols,
                    'final_symbols': final_symbols, 'map_text': map_text}
