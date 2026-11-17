"""Synthetic CMake structure/guard checks, not production compiler qualification.

The toy config mocks declared platform/compiler/generator identity AFTER project()
so local compilers can test deferred target/property behavior. It never builds.
Actual production identity remains mandatory and is not overridable by overlay.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

CMAKE = os.environ.get('CS80_RESEARCH_CMAKE') or shutil.which('cmake')
OVERLAY = Path(__file__).with_name('overlay.cmake').resolve()
TEMPLATE = '''cmake_minimum_required(VERSION 3.22)
project(CS80Reference LANGUAGES CXX)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CS80_BUILD_PLUGIN ON)
set(CS80_BUILD_TESTS ON)
set(CS80_ENABLE_AVX2_FILTER_BATCH ON)
set(CS80_ENABLE_EXPERIMENTAL_FILTER_RECOVERY OFF)
add_library(cs80_core STATIC src/core/Engine.cpp src/core/FactoryPresets.cpp src/core/Ig00156Batch.cpp)
add_library(CS80Reference_VST3 MODULE Plugin.cpp)
add_executable(CS80Reference_Standalone Main.cpp)
target_link_libraries(CS80Reference_VST3 PRIVATE cs80_core)
target_link_libraries(CS80Reference_Standalone PRIVATE cs80_core)
# Only toy metadata is mocked. No native build or training happens.
set(CMAKE_CXX_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_VERSION 13.3.0)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_GENERATOR Ninja)
{extra}
'''


@unittest.skipUnless(CMAKE, 'Set CS80_RESEARCH_CMAKE for synthetic CMake guard tests')
class OverlayTests(unittest.TestCase):
    def configure(self, root, mode, extra='', overlay=True):
        source = root / 'source'; (source / 'src/core').mkdir(parents=True, exist_ok=True)
        for name in ('Engine', 'FactoryPresets', 'Ig00156Batch'):
            (source / 'src/core' / (name + '.cpp')).write_text('int ' + name + '() { return 1; }\n')
        (source / 'Plugin.cpp').write_text('int plugin() { return 0; }\n')
        (source / 'Main.cpp').write_text('int main() { return 0; }\n')
        (source / 'CMakeLists.txt').write_text(TEMPLATE.format(extra=extra))
        return subprocess.run([CMAKE, '-S', str(source), '-B', str(root / 'build'),
            '-G', 'Unix Makefiles', '-DCMAKE_BUILD_TYPE=Release',
            '-DCMAKE_PROJECT_INCLUDE=' + (str(OVERLAY) if overlay else ''),
            '-DCS80_PIC_PGO_MODE=' + mode,
            '-DCS80_PIC_PGO_PROFILE_DIR=' + str(root / 'profiles')], capture_output=True, text=True)

    def test_deferred_modes_and_engine_only_commands(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); databases = {}
            for mode in ('baseline', 'generate', 'use'):
                result = self.configure(root, mode)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                databases[mode] = json.loads((root / 'build/compile_commands.json').read_text())
            for mode in ('generate', 'use'):
                for a, b in zip(databases['baseline'], databases[mode]):
                    self.assertEqual(a['file'], b['file'])
                    if a['file'].endswith('/Engine.cpp'):
                        self.assertIn('-fPIC', b['command'])
                        self.assertIn('-fprofile-' + mode, b['command'])
                    else: self.assertEqual(a['command'], b['command'])
            link = (root / 'build/CMakeFiles/CS80Reference_VST3.dir/link.txt').read_text()
            self.assertIn('.research.map,--cref', link)
            self.assertNotIn('-fprofile-', link)

    def test_without_overlay_no_options_or_maps(self):
        with tempfile.TemporaryDirectory() as t:
            root = Path(t); result = self.configure(root, 'use', overlay=False)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertNotIn('-fprofile-', (root / 'build/compile_commands.json').read_text())
            self.assertNotIn('.research.map', (root / 'build/CMakeFiles/CS80Reference_VST3.dir/link.txt').read_text())

    def test_unsupported_identity_and_configuration_rejected(self):
        for extra in ('set(CMAKE_CXX_COMPILER_VERSION 14.2.0)', 'set(CMAKE_SYSTEM_NAME Darwin)',
                      'set(CMAKE_GENERATOR Other)', 'set(CS80_ENABLE_AVX2_FILTER_BATCH OFF)',
                      'set_property(TARGET cs80_core PROPERTY POSITION_INDEPENDENT_CODE OFF)',
                      'set_property(TARGET cs80_core PROPERTY INTERPROCEDURAL_OPTIMIZATION ON)',
                      'set_property(TARGET cs80_core PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)',
                      'set_property(SOURCE src/core/Engine.cpp PROPERTY COMPILE_OPTIONS -ffast-math)'):
            with self.subTest(extra=extra), tempfile.TemporaryDirectory() as t:
                result = self.configure(Path(t), 'use', extra)
                self.assertNotEqual(result.returncode, 0)

    def test_unrecognized_mode_rejected(self):
        with tempfile.TemporaryDirectory() as t:
            self.assertNotEqual(self.configure(Path(t), 'automatic').returncode, 0)

if __name__ == '__main__': unittest.main()
