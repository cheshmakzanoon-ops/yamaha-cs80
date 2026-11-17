"""Production CMake build/provenance operations for the bounded trial draft.

Only the reviewed orchestrator may invoke these operations. Import is inert.
"""
import importlib.util
import json
from pathlib import Path
import shlex

import host_contract as host
from preflight import cache_values, tree_manifest, test_inventory
from provenance import require, sha256, verify_compile_scope, command_entries, frozen_files, verify_frozen

ROOT = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location('original_strict_pgo', ROOT.parent / 'strict-pgo-research/run.py')
STRICT = importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(STRICT)
VERIFICATION_OBJECTS = {
    'scripts': '7f93b589a5a591da004d1d55378f4ca448d7d889',
    '.github/workflows/build.yml': '7b577e8c59328a8171c6fe26a4020a9d7a05e1fe',
}
FROZEN_INPUTS = {**STRICT.OBJECTS, **VERIFICATION_OBJECTS}

CORE_TESTS = (
    ('cs80_smoothing_tests', 120), ('cs80_active_tail_continuity_tests', 120),
    ('cs80_mpe_master_smoothing_tests', 120), ('cs80_source_performance_audit_tests', 180),
    ('cs80_filter_batch_engine_tests', 180),
)
TARGETS = ('CS80Reference_VST3', 'CS80Reference_Standalone', 'cs80_source_factory_benchmark',
           'cs80_vst3_sub_automation_tests', 'cs80_vst3_ring_automation_tests',
           *(name for name, _ in CORE_TESTS))


def save(path, value):
    Path(path).write_text(json.dumps(value, indent=2) + '\n')


def qualification_compile_argv(original, repo, source, obj):
    argv = list(original)
    index = argv.index('-o'); argv[index + 1] = str(obj)
    index = argv.index('-c')
    require(argv[index + 1] == str(Path(repo) / 'src/tools/source_factory_benchmark.cpp'), 'driver source anchor')
    argv[index + 1] = str(source)
    # Relocation loses the original quoted-include search directory.
    argv[1:1] = ['-I' + str(ROOT.parent / 'strict-pgo-research'), '-I' + str(Path(repo) / 'src/tools')]
    return argv


class Production:
    def __init__(self, repo, work, out, commands):
        self.repo, self.work, self.out = map(Path, (repo, work, out))
        self.commands = commands
        self.build = self.work / 'build'
        self.profiles = self.work / 'profiles'; self.profiles.mkdir()
        self.engine = self.build / 'CMakeFiles/cs80_core.dir/src/core/Engine.cpp.o'
        self.archive = self.build / 'libcs80_core.a'
        self.benchmark = self.build / 'cs80_source_factory_benchmark'
        self.bundle = self.build / 'CS80Reference_artefacts/Release/VST3/CS-80 Reference.vst3'
        self.module = host.module_path(self.build)
        self.standalone = self.build / 'CS80Reference_artefacts/Release/Standalone/CS-80 Reference'
        self.maps = {name: self.build / (name + '.research.map')
                     for name in ('CS80Reference_VST3', 'CS80Reference_Standalone')}
        self.baseline_commands = None
        self.dependency_identity = None
        self.test_definitions = None
        self.config_inventory = None
        self.compiler = None
        self.ordinary = None
        self.fp_options = None

    def run(self, label, argv, **kwargs):
        return self.commands.run(label, argv, cwd=self.repo, **kwargs)

    def output(self, label, argv, **kwargs):
        stdout, _ = self.run(label, argv, **kwargs)
        return stdout.read_text()

    def verify_source(self, label):
        for path, expected in FROZEN_INPUTS.items():
            observed = self.output(label + '-git-' + path.replace('/', '-'),
                                   ['git', 'rev-parse', 'HEAD:' + path]).strip()
            require(observed == expected, 'production source object drift ' + path)
        require(not self.output(label + '-source-clean', ['git', 'status', '--porcelain', '--', *FROZEN_INPUTS]).strip(),
                'production source dirty')

    def configure(self, mode):
        require(mode in ('baseline', 'generate', 'use'), 'unknown production mode')
        self.run('configure-' + mode, ['cmake', '-S', self.repo, '-B', self.build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', '-DCS80_BUILD_PLUGIN=ON', '-DCS80_BUILD_TESTS=ON',
            '-DCS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY=OFF', '-DCS80_ENABLE_AVX2_FILTER_BATCH=ON',
            '-DCMAKE_PROJECT_INCLUDE=' + str(ROOT / 'overlay.cmake'),
            '-DCS80_PIC_PGO_MODE=' + mode, '-DCS80_PIC_PGO_PROFILE_DIR=' + str(self.profiles)])
        self.run('juce-verify-' + mode, ['cmake', '-DBUILD_DIR=' + str(self.build),
            '-DEXPECTED_JUCE_SHA=c352e2489000ffeb9e397051377d75eef2efe641', '-DEXPECTED_JUCE_VERSION=8.0.11',
            '-P', self.repo / 'scripts/verify-ci-juce-source.cmake'])
        cache_text = (self.build / 'CMakeCache.txt').read_text()
        (self.out / (mode + '-CMakeCache.txt')).write_text(cache_text)
        cache = cache_values(cache_text)
        expected = json.loads((ROOT / 'frozen-compiler.json').read_text())
        identity = {}
        for language, frontend_name in [('C', 'cc1'), ('CXX', 'cc1plus')]:
            driver = cache['CMAKE_' + language + '_COMPILER']
            version = self.output(mode + '-' + language + '-version', [driver, '-dumpfullversion']).strip()
            frontend = self.output(mode + '-' + language + '-frontend', [driver, '-print-prog-name=' + frontend_name]).strip()
            require(Path(frontend).is_absolute() and Path(frontend).is_file(), 'absolute compiler frontend required')
            identity[language] = {'command': driver, 'resolved': str(Path(driver).resolve()),
                'sha256': sha256(Path(driver).resolve()), 'version': version,
                'frontend': str(Path(frontend).resolve()), 'frontend_sha256': sha256(frontend)}
        require(identity == expected, 'compiler differs from actual reviewed GNU13.3 preflight')
        self.compiler = identity['CXX']['command']
        save(self.out / (mode + '-compiler.json'), identity)
        database = json.loads((self.build / 'compile_commands.json').read_text())
        require(len(database) == 587, 'actual preflight 587-entry compile roster changed')
        if self.baseline_commands is None:
            require(mode == 'baseline', 'baseline must configure first')
            self.baseline_commands = database
        verify_compile_scope(self.baseline_commands, database, self.repo, self.profiles, mode,
                             expected_compiler=self.compiler, expected_c_compiler=identity['C']['command'])
        save(self.out / (mode + '-compile_commands.json'), database)
        engine_row = next(row for row in command_entries(database).values() if row['source'] == str(self.repo / 'src/core/Engine.cpp'))
        options = list(engine_row['argv'])
        for switch in ('-o', '-c'):
            index = options.index(switch); del options[index:index + 2]
        states = STRICT.optimizer_states(self.output(mode + '-FP-options', [*options, '-Q', '--help=optimizers']))
        if self.fp_options is None: self.fp_options = states
        require(states == self.fp_options, 'production FP optimizer state changed')
        save(self.out / (mode + '-FP-options.json'), states)
        dependencies = {}
        juce = self.repo / 'deps/juce' if (self.repo / 'deps/juce/CMakeLists.txt').exists() else self.build / '_deps/juce-src'
        libxml = self.build / '_deps/cs80_libxml2-src'
        for name, path, commit in [('juce', juce, 'c352e2489000ffeb9e397051377d75eef2efe641'),
                                  ('libxml2', libxml, '96498992efa48d52b0e8b83058bd88dbdaf153c1')]:
            require(self.output(mode + '-' + name + '-head', ['git', '-C', path, 'rev-parse', 'HEAD']).strip() == commit,
                    name + ' dependency revision drift')
            dependencies[name] = tree_manifest(path)
            self.output(mode + '-' + name + '-diff', ['git', '-C', path, 'diff', '--binary', 'HEAD'])
        if self.dependency_identity is None: self.dependency_identity = dependencies
        require(dependencies == self.dependency_identity, 'patched dependency content changed across configurations')
        save(self.out / (mode + '-dependencies.json'), dependencies)
        inventory = json.loads(self.output('inventory-' + mode, ['ctest', '--test-dir', self.build, '--show-only=json-v1']))
        normalized = test_inventory(inventory)
        if self.config_inventory is None: self.config_inventory = inventory
        else:
            previous = test_inventory(self.config_inventory)
            # Previously absent executable commands may resolve after baseline build.
            require(previous.keys() == normalized.keys(), 'configured test roster drift')
            for name in previous:
                require(previous[name]['properties'] == normalized[name]['properties'], 'configured test properties drift')
                if previous[name]['command'] is not None:
                    require(previous[name]['command'] == normalized[name]['command'], 'configured test command drift')
        save(self.out / (mode + '-test-inventory.json'), inventory)
        definitions = host.definitions_manifest(self.build)
        if self.test_definitions is None: self.test_definitions = definitions
        require(definitions == self.test_definitions, 'raw CTest definitions changed')
        save(self.out / (mode + '-CTest-definitions.json'),
             {str(path.relative_to(self.build)): {'sha256': sha256(path), 'text': path.read_text()}
              for path in sorted(self.build.rglob('CTestTestfile.cmake'))})
        for target in ('cs80_core', 'CS80Reference_VST3', 'CS80Reference_Standalone'):
            self.output(mode + '-ninja-commands-' + target, ['ninja', '-C', self.build, '-t', 'commands', target])
        return database

    def build_targets(self, label, targets):
        self.run(label, ['cmake', '--build', self.build, '--verbose', '--parallel', '2', '--target', *targets])
        host.verify_definitions(self.test_definitions, self.build)

    def snapshot_ordinary(self):
        entries = command_entries(self.baseline_commands)
        existing = [Path(path) for path in entries if Path(path).is_file() and Path(path) != self.engine]
        require(existing, 'no ordinary production objects built')
        self.ordinary = frozen_files(existing)
        save(self.out / 'ordinary-objects.json', self.ordinary)

    def unchanged_ordinary(self):
        require(self.ordinary is not None, 'ordinary snapshot missing')
        verify_frozen(self.ordinary)

    def remove_owned_outputs(self, paths):
        """Only exact existing generated outputs under this fresh work directory."""
        for path in map(Path, paths):
            require(path.resolve().is_relative_to(self.work.resolve()) and path.is_file() and not path.is_symlink(),
                    'expected owned regular build output required')
            path.unlink()
            require(not path.exists(), 'stale build output remains')

    def build_qualification_driver(self):
        entries = command_entries(self.baseline_commands)
        drivers = [row for row in entries.values() if row['source'] == str(self.repo / 'src/tools/source_factory_benchmark.cpp')]
        require(len(drivers) == 1, 'one normal benchmark driver required')
        source = self.work / 'qualification.cpp'; obj = self.work / 'qualification.o'
        source.write_text(STRICT.qualification_source((self.repo / 'src/tools/source_factory_benchmark.cpp').read_text()))
        argv = qualification_compile_argv(drivers[0]['argv'], self.repo, source, obj)
        self.commands.run('compile-qualification-driver-once', argv, cwd=self.build)
        return obj

    def link_extra(self, label, driver_object, output, *, training=False):
        argv = [self.compiler]
        if training: argv += ['-fprofile-generate=' + str(self.profiles)]
        argv += [driver_object, self.archive, '-o', output]
        self.run(label, argv)
