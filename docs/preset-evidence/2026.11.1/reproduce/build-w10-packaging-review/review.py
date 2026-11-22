#!/usr/bin/env python3
"""Independent bounded packaging review; no native products or network calls."""
from pathlib import Path
import hashlib
import importlib.util
import json
import re
import subprocess
import sys
import tempfile
from urllib.parse import unquote, urljoin, urlsplit, urlunsplit

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
OUT = Path(__file__).resolve().parent
SOURCE = 'be460b0b93bdcb8dce5eb8925b18b72706b25e19'
PREFIX = f'https://github.com/cheshmakzanoon-ops/yamaha-cs80/blob/{SOURCE}/'
GUIDES = {
    'docs/INSTALL_AND_QUICKSTART.md': 'INSTALL_AND_QUICKSTART.md',
    'docs/PRESET_GUIDE.md': 'PRESET_GUIDE.md',
    'docs/CI_DISTRIBUTION_STATUS.md': 'DISTRIBUTION_STATUS.md',
}
LINK = re.compile(r'\[[^\]\n]*\]\(([^\s)]+)(?:\s+"[^"]*")?\)')
spec = importlib.util.spec_from_file_location('reviewed_packager', ROOT / 'scripts/package-ci-artifacts.py')
packager = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packager)
checks = []


def check(condition, label):
    if not condition:
        raise AssertionError(label)
    checks.append(label)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def main():
    source_files = set(subprocess.check_output(
        ['git', 'ls-tree', '-r', '--name-only', SOURCE], cwd=ROOT, text=True).splitlines())
    inputs = {}
    reviewed = ['scripts/package-ci-artifacts.py', 'scripts/test-packaging-notices.py',
                'src/core/FactoryPresets.h', *GUIDES]
    for name in reviewed:
        data = (ROOT / name).read_bytes()
        committed = subprocess.check_output(['git', 'show', f'{SOURCE}:{name}'], cwd=ROOT)
        check(data == committed, f'{name}: unchanged since binary source')
        inputs[name] = digest(data)

    links, categories = [], {'commit_pinned': 0, 'local_guide': 0, 'external': 0, 'fragment': 0}
    for source, destination in GUIDES.items():
        original = (ROOT / source).read_text()
        packaged = packager.packaged_markdown(source, SOURCE, GUIDES).decode()
        before, after = LINK.findall(original), LINK.findall(packaged)
        check(len(before) == len(after), f'{source}: link count preserved')
        for old, new in zip(before, after):
            parsed = urlsplit(old)
            if parsed.scheme or parsed.netloc:
                expected, kind = old, 'external'
            elif not parsed.path:
                expected, kind = old, 'fragment'
            else:
                # URI resolution is independent of the packager's POSIX join.
                resolved = urljoin(PREFIX + source, old)
                resolved_parts = urlsplit(resolved)
                repo_path = unquote(resolved_parts.path.split('/blob/' + SOURCE + '/', 1)[1])
                check(repo_path in source_files, f'{source}: target exists at binary source: {repo_path}')
                if repo_path in GUIDES:
                    expected = urlunsplit(('', '', GUIDES[repo_path], parsed.query, parsed.fragment))
                    kind = 'local_guide'
                else:
                    expected, kind = resolved, 'commit_pinned'
            check(new == expected, f'{source}: exact target for {old}')
            categories[kind] += 1
            links.append({'guide': destination, 'original': old, 'packaged': new, 'kind': kind})
    check(len(links) == 47, 'all 47 real guide links observed')

    with tempfile.TemporaryDirectory(prefix='cs80-packaging-review-') as temporary:
        temporary = Path(temporary)
        (temporary / 'docs').mkdir()
        (temporary / 'src/core').mkdir(parents=True)
        saved = packager.ROOT
        packager.ROOT = temporary
        try:
            header = temporary / 'src/core/FactoryPresets.h'
            header.write_bytes(b'inline constexpr const char* curatedLibraryRevision = "2026.11.1";\r\n')
            check(packager.read_preset_library_revision() == '2026.11.1', 'CRLF revision declaration accepted')
            header.write_bytes(header.read_bytes() * 2)
            try:
                packager.read_preset_library_revision()
                raise AssertionError('duplicate CRLF declarations accepted')
            except ValueError:
                checks.append('duplicate CRLF revision declarations rejected')

            source = (
                '[Status](./CI_DISTRIBUTION_STATUS.md?plain=1#state)\r\n'
                '[Code](../src/with%20space/Caf%C3%A9.cpp#L2)\r\n'
                '[External](https://example.invalid/a?q=1#x) [Anchor](#here)\r\n'
                '````md\r\n[Example](../keep.md)\r\n```\r\n'
                '[Still example](../keep-too.md)\r\n````\r\n'
                '[After](../src/core/Engine.cpp)\r\n')
            expected = (
                '[Status](DISTRIBUTION_STATUS.md?plain=1#state)\n'
                f'[Code]({PREFIX}src/with%20space/Caf%C3%A9.cpp#L2)\n'
                '[External](https://example.invalid/a?q=1#x) [Anchor](#here)\n'
                '````md\n[Example](../keep.md)\n```\n'
                '[Still example](../keep-too.md)\n````\n'
                f'[After]({PREFIX}src/core/Engine.cpp)\n')
            guide = temporary / 'docs/PRESET_GUIDE.md'
            guide.write_bytes(source.encode())
            check(packager.packaged_markdown('docs/PRESET_GUIDE.md', SOURCE, GUIDES).decode() == expected,
                  'CRLF, encoded Unicode/space, local rename, query/fragment, external URL and four-backtick fence fixture')
            for target in ('../../outside.md', '%2e%2e/%2e%2e/outside.md', '..%5Coutside.md'):
                guide.write_text(f'[Outside]({target})\n')
                try:
                    packager.packaged_markdown('docs/PRESET_GUIDE.md', SOURCE, GUIDES)
                    raise AssertionError('unsafe target accepted: ' + target)
                except ValueError:
                    checks.append('unsafe relative target rejected: ' + target)
        finally:
            packager.ROOT = saved

    summary = {
        'assignment': 46, 'result': 'PASS', 'blocking_findings': [],
        'binary_source_commit': SOURCE, 'pending_build_run': 45042166237,
        'scope': 'Independent review of the two packaging changes and install guide; production read-only.',
        'existing_fixture_results': {'notice_guide_test_methods': 7, 'synthetic_platform_shapes': 4,
                                     'packager_fixture_rejection_checks': 46},
        'independent_check_count': len(checks), 'checks': checks,
        'actual_guide_links': len(links), 'link_categories': categories, 'links': links,
        'findings': [
            'Canonical calendar and positive-patch syntax remains strict; malformed month/patch/trailing/duplicate declarations are rejected.',
            'All real guide links resolve to the intended existing file at the exact binary source, or to a bundled guide/external URL.',
            'PurePosixPath, posixpath and URI operations keep packaged paths independent of host path separators; CRLF input is handled by text reading.',
            'Only the three project guide payloads are transformed. License/notice bytes continue through the unchanged raw-byte paths and are verified inside all four synthetic archive shapes.',
            'Source cleanliness, GITHUB_SHA and pinned JUCE evidence checks remain intact; package inventories hash the actual transformed bytes.',
            'The corrected 67-sample default Circuit Standard guide statement agrees with existing core and native test contracts.',
        ],
        'limitations': [
            'These are local synthetic package and source/link checks, not verification of pending CI-produced native downloads.',
            'Native Windows/macOS execution is not claimed; their archive shapes and host-independent path construction are covered.',
            'The transformer intentionally covers the inline-link syntax in current project guides, not every Markdown construct.',
            'No runtime test rerun was needed for the guide-only latency wording correction.',
        ],
        'input_sha256': inputs,
        'artifacts': {name: digest((OUT / name).read_bytes()) for name in
                      ('notices-tests.log', 'package-self-test.log', 'review.py')},
    }
    (OUT / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')
    print(json.dumps({'result': 'PASS', 'checks': len(checks), 'links': len(links),
                      'categories': categories, 'summary_sha256': digest((OUT / 'summary.json').read_bytes())}, indent=2))


if __name__ == '__main__':
    main()
