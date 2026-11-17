import copy
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location('linkage', Path(__file__).with_name('linkage.py'))
p = importlib.util.module_from_spec(spec); spec.loader.exec_module(p)


def fixture():
    return {'engine_bytes': b'frozen candidate object', 'archive_member_bytes': b'frozen candidate object',
            'archive_members': ['Engine.cpp.o', 'FactoryPresets.cpp.o', 'Ig00156Batch.cpp.o'],
            'archive_path': '/build/libcs80_core.a', 'cwd': '/build', 'final_bytes': b'frozen plugin ELF',
            'object_symbols': '0000000000000020 g F .text 0000000000000040 ' + '_ZN4cs806Engine6renderEPfiPKf',
            'final_symbols': '0000000000001020 g F .text 0000000000000040 ' + '_ZN4cs806Engine6renderEPfiPKf',
            'map_text': 'Discarded input sections\n .text 0x0 0x99 /other/lib.a(Engine.cpp.o)\n'
                        'Linker script and memory map\n .text 0x1000 0x100 libcs80_core.a(Engine.cpp.o)\n'
                        'Cross Reference Table\n'}


class LinkageTests(unittest.TestCase):
    def test_correct_live_member_not_discarded(self):
        report = p.verify_linkage(**fixture())
        self.assertEqual(report['live_input_section']['address'], 4096)
        self.assertEqual(report['engine_sha256'], report['archive_member_sha256'])

    def test_real_three_argument_render_signature(self):
        self.assertEqual(p.RENDER, '_ZN4cs806Engine6renderEPfiPKf')
        p.verify_linkage(**fixture())
        x = fixture()
        for field in ('object_symbols', 'final_symbols'):
            x[field] = x[field].replace('_ZN4cs806Engine6renderEPfiPKf', '_ZN4cs806Engine6renderEPfi')
        with self.assertRaises(ValueError): p.verify_linkage(**x)

    def test_hidden_and_local_render_records(self):
        x = fixture()
        x['object_symbols'] = x['object_symbols'].replace(' _ZN', ' .hidden _ZN')
        x['final_symbols'] = x['final_symbols'].replace(' g F ', ' l F ')
        p.verify_linkage(**x)
        x['final_symbols'] = x['final_symbols'].replace(' _ZN', ' .unknown _ZN')
        with self.assertRaises(ValueError): p.verify_linkage(**x)

    def test_wrapped_map_section(self):
        x = fixture(); x['map_text'] = x['map_text'].replace(' .text 0x1000', ' .text\n                0x1000')
        p.verify_linkage(**x)

    def test_wrong_or_duplicate_archive_member(self):
        for field, value in [('archive_member_bytes', b'baseline'), ('engine_bytes', b''),
                             ('archive_members', ['Engine.cpp.o', 'Engine.cpp.o']),
                             ('archive_members', ['FactoryPresets.cpp.o']),
                             ('archive_path', '/other/libcs80_core.a')]:
            x = fixture(); x[field] = value
            with self.subTest(field=field), self.assertRaises(ValueError): p.verify_linkage(**x)

    def test_stale_map_and_symbol_size(self):
        for field, old, new in [('map_text', '0x1000', '0x2000'),
                                ('final_symbols', '0000000000000040', '0000000000000041'),
                                ('map_text', '0x100 libcs80', '0x40 libcs80')]:
            x = fixture(); x[field] = x[field].replace(old, new)
            with self.assertRaises(ValueError): p.verify_linkage(**x)

    def test_discarded_only_not_proof(self):
        x = fixture(); x['map_text'] = x['map_text'].replace(' .text 0x1000 0x100 libcs80_core.a(Engine.cpp.o)\n', '')
        with self.assertRaises(ValueError): p.verify_linkage(**x)

    def test_duplicate_or_direct_provider(self):
        for contribution in (' .text 0x2000 0x100 /other/lib.a(Engine.cpp.o)\n',
                             ' .text 0x2000 0x100 /other/Engine.cpp.o\n'):
            x = fixture(); x['map_text'] = x['map_text'].replace('Cross Reference Table', contribution + 'Cross Reference Table')
            with self.assertRaises(ValueError): p.verify_linkage(**x)

    def test_missing_duplicate_or_undefined_symbol(self):
        for field in ('object_symbols', 'final_symbols'):
            for change in ('empty', 'duplicate', 'undefined'):
                x = fixture()
                if change == 'empty': x[field] = ''
                elif change == 'duplicate': x[field] += '\n' + x[field]
                else: x[field] = x[field].replace('.text', '*UND*')
                with self.subTest(field=field, change=change), self.assertRaises(ValueError): p.verify_linkage(**x)

    def test_instrumentation_and_missing_final_bytes(self):
        x = fixture(); x['final_symbols'] += '\n__gcov_init'
        with self.assertRaises(ValueError): p.verify_linkage(**x)
        x = fixture(); x['final_bytes'] = b''
        with self.assertRaises(ValueError): p.verify_linkage(**x)

if __name__ == '__main__': unittest.main()
