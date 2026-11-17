"""Strict parser for the observed generated CTest add_test literal subset.

Not a general CMake interpreter. Reject variable/generator expansion, escapes,
list expansion, multiline or unquoted forms instead of guessing their meaning.
All 569 current generated commands use this literal subset.
"""
import re
from identity import require

LINE=re.compile(r'add_test\(\[=\[([^\]\r\n]+)\]=\]((?: "[^"\\;$\r\n]*")+)\)')
ARG=re.compile(r' "([^"\\;$\r\n]*)"')


def commands_from_definitions(texts):
    require(isinstance(texts,dict) and bool(texts), 'complete definition texts required')
    result={}
    for path,text in texts.items():
        require(isinstance(path,str) and isinstance(text,str), 'definition text required')
        for line in text.splitlines():
            if not line.lstrip().lower().startswith('add_test'):continue
            m=LINE.fullmatch(line)
            require(m is not None, 'unsupported generated add_test syntax')
            name,args=m.groups()
            require(name not in result, 'duplicate raw test definition')
            argv=ARG.findall(args)
            require(argv and all(argv), 'empty raw test argument unsupported')
            result[name]=argv
    require(bool(result), 'missing raw add_test definitions')
    return result


def bind_definition_commands(texts, built_commands, *, executable_resolutions):
    from pathlib import PurePosixPath
    raw=commands_from_definitions(texts)
    require(set(raw)==set(built_commands), 'raw/resolved test inventory differs')
    relative={argv[0] for argv in raw.values() if not PurePosixPath(argv[0]).is_absolute()}
    require(isinstance(executable_resolutions,dict) and set(executable_resolutions)==relative,
            'exact frozen relative-executable resolutions required')
    for name,argv in raw.items():
        expected=list(argv)
        if expected[0] in relative:
            target=executable_resolutions[expected[0]]
            require(isinstance(target,str) and PurePosixPath(target).is_absolute() and '..' not in PurePosixPath(target).parts,
                    'canonical resolved executable required')
            expected[0]=target
        require(expected==built_commands[name], 'case-to-argv differs from raw definition: '+name)
    return {'raw_case_commands_bound':len(raw),'executable_resolution_provenance_required':bool(relative)}
