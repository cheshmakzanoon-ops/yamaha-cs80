import importlib.util
from pathlib import Path
import sys
import unittest
sys.path.insert(0,str(Path(__file__).parent))
spec=importlib.util.spec_from_file_location('fixture',Path(__file__).with_name('compiler_path_fixture_v2.py'))
p=importlib.util.module_from_spec(spec);spec.loader.exec_module(p)

class FixtureTests(unittest.TestCase):
    def test_specific_errors_not_verbose_flags(self):
        for warning in ('missing-profile','coverage-mismatch'):
            p.require_compiler_error('Engine.cpp:1:1: error: profile problem [-Werror='+warning+']\n',warning)
            for bad in ('c++ -Werror=missing-profile -Werror=coverage-mismatch -c Engine.cpp\nFAILED: Engine.cpp.o\n',
                        'error: unrelated compile failure\n',
                        'warning: profile problem [-Werror='+warning+']\n'):
                with self.assertRaises(ValueError):p.require_compiler_error(bad,warning)
    def test_fixture_is_tiny_integer_only_relative_cmake_target(self):
        self.assertIn('add_library(cs80_core STATIC src/core/Engine.cpp)',p.CMAKE)
        self.assertIn('fixture_value',p.ORIGINAL)
        self.assertNotIn('cs80::',p.ORIGINAL)
        self.assertNotEqual(p.ORIGINAL,p.MISMATCH)
        self.assertIn('CMAKE_CXX_COMPILER_VERSION VERSION_EQUAL "13.3.0"',p.CMAKE)

if __name__=='__main__':unittest.main()
