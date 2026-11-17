"""Parse actual current CMake compile inputs; never compile or execute commands."""
from pathlib import Path
import shlex
from identity import require, capture_files


def absolute(value, directory):
    path = Path(value)
    return path if path.is_absolute() else directory / path


def compile_inventory(database, *, source_root, build_root):
    """Return exact original argv and all object output paths.

    Intended for the single Unix Make/Ninja native Linux qualification. Compiler
    launchers, response files, combined switches and nonliteral working paths
    need separately reviewed support, not heuristic guessing. All compile rows
    must correspond to built regular files before capture_objects is called.
    """
    require(isinstance(database, list) and bool(database), 'nonempty compile database required')
    source_root, build_root = Path(source_root).absolute(), Path(build_root).absolute()
    result = {}
    for row in database:
        require(isinstance(row, dict), 'compile row required')
        directory = Path(row.get('directory', ''))
        require(directory.is_absolute() and directory.is_relative_to(build_root) and '..' not in directory.parts, 'literal owned build cwd required')
        if 'arguments' in row:
            argv = row['arguments']
            require(isinstance(argv, list) and argv and all(isinstance(s, str) and s for s in argv),
                    'invalid compile argv')
            if 'command' in row:
                require(shlex.split(row['command']) == argv, 'conflicting compile command forms')
        else:
            require(isinstance(row.get('command'), str), 'missing compile command')
            argv = shlex.split(row['command'])
        require(argv and Path(argv[0]).is_absolute(), 'explicit compiler required')
        require(not any(s.startswith('@') for s in argv), 'response files require explicit reviewed expansion')
        require(argv.count('-o') == argv.count('-c') == 1, 'one compile source/output required')
        oi, ci = argv.index('-o'), argv.index('-c')
        require(oi+1 < len(argv) and ci+1 < len(argv), 'missing source/output operand')
        source = absolute(argv[ci+1], directory)
        output = absolute(argv[oi+1], directory)
        require('..' not in source.parts and '..' not in output.parts, 'noncanonical source/output path')
        require(output.is_relative_to(build_root) and output != build_root, 'object outside build')
        require(source.is_relative_to(source_root) or source.is_relative_to(build_root), 'source outside frozen roots')
        require(isinstance(row.get('file'),str) and absolute(row['file'],directory) == source, 'source metadata differs')
        if 'output' in row:
            require(absolute(row['output'],directory) == output, 'output metadata differs')
        relative = output.relative_to(build_root).as_posix()
        require(relative not in result, 'duplicate object output')
        result[relative] = {'source':str(source),'directory':str(directory),'arguments':argv}
    return result


def capture_objects(database, *, source_root, build_root):
    rows = compile_inventory(database, source_root=source_root, build_root=build_root)
    return {'inventory':rows,'sha256':capture_files(build_root,list(rows))}
