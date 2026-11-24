#!/usr/bin/env python3
"""Bounded offline fixtures; historical bytes are format samples, never .7 passes."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
REPO = HERE.parents[4]
spec = importlib.util.spec_from_file_location('a47_runtime', HERE / 'verify-linux-runtime-evidence.py')
m = importlib.util.module_from_spec(spec); spec.loader.exec_module(m)
OLD = REPO / 'docs/preset-evidence/2026.11.4/ci/source-078c098'
MEMBERS = OLD / 'windows-linux-validators/linux-original-validator-files'
CONTRACT = json.loads((HERE / 'acceptance-contract.json').read_text())


def core(pid, raw):
    status = raw if raw >= 0 else 128 - raw
    detail = f'kind=exit; exit_code={raw}' if raw >= 0 else 'kind=signal; signal_number=11; signal_name=SIGSEGV'
    return (f'=== validator attempt: local core diagnostics ===\nOriginal validator PID: {pid}; exit: {status}\n'
            f'Original validator termination: PID={pid}; raw returncode={raw}; {detail}\n').encode()


class RuntimeChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.members = {str(p.relative_to(MEMBERS)): p.read_bytes() for p in MEMBERS.rglob('*') if p.is_file()}
        cls.groups = m.log_groups((OLD / 'linux-x86_64/linux-x86_64-pluginval-job.log').read_bytes())
        cls.job = json.loads((OLD / 'linux-x86_64/linux-x86_64-pluginval-job-api.json').read_text())
        cls.workspace = '/home/runner/work/yamaha-cs80/yamaha-cs80'

    def step(self, lane):
        first = m.lines(self.members[lane['console']])
        candidates = [g for g in self.groups if m.messages(g) == first]
        self.assertEqual(len(candidates), 1)
        api = next(s for s in self.job['steps'] if s['name'] == lane['step'])
        return dict(api=api, group=copy.deepcopy(candidates[0]))

    def test_01_historical_three_lane_format_only(self):
        for lane in CONTRACT['lanes']:
            result = m.lane_binding(lane, self.members, self.step(lane), self.workspace)
            self.assertEqual(result['invocations'], 1)
            self.assertTrue(result['original_normal_exit_zero'])

    def test_02_source_commands_and_boundaries(self):
        original = m.SOURCE
        try:
            # Context override only inside this synthetic format test. No result
            # is written or relabeled as an ee0dabca observation.
            m.SOURCE = self.job['head_sha']
            result = m.bind_steps(self.job, self.groups, CONTRACT['source_steps']['pluginval'])
            self.assertEqual(result['workspace'], self.workspace)
            mutated = copy.deepcopy(self.groups)
            target = next(g for g in mutated if '--strictness-level 10 \\' in '\n'.join(g['command']))
            target['command'] = [line.replace('--strictness-level 10', '--strictness-level 9') for line in target['command']]
            with self.assertRaises(ValueError):
                m.bind_steps(self.job, mutated, CONTRACT['source_steps']['pluginval'])
        finally:
            m.SOURCE = original

    def test_03_exact_retry_predicate_and_raw_distinction(self):
        valid = b'Starting tests in: fixture\nCompleted tests in: fixture\n'
        self.assertTrue(m.retry_eligible(valid, dict(normalized_exit=139)))
        for body, status in [(valid.replace(b'in:', b'in', 2), 139),
                             (valid + b'Starting tests in: pending\n', 139),
                             (valid + b'SUCCESS\n', 139), (valid + b'FAILED\n', 139),
                             (b'Completed tests in: no-start\n', 139), (valid, 0)]:
            with self.subTest(body=body, status=status):
                self.assertFalse(m.retry_eligible(body, dict(normalized_exit=status)))
        exited = m.terminations(core(10, 139))[0]
        signaled = m.terminations(core(11, -11))[0]
        self.assertEqual(exited['normalized_exit'], signaled['normalized_exit'])
        self.assertEqual((exited['kind'], signaled['kind']), ('exit', 'signal'))
        for bad in [core(10, 0).replace(b'PID=10', b'PID=12'),
                    core(10, 139).replace(b'exit: 139', b'exit: 0'),
                    core(10, 139).replace(b'kind=exit', b'kind=signal'),
                    core(10, 0) + core(10, 0)]:
            with self.assertRaises(ValueError):
                m.terminations(bad)

    def test_04_preserved_retry_and_original_warning(self):
        lane = CONTRACT['lanes'][0]
        data = dict(self.members)
        name = lane['console'][:-4] + '-attempt1.log'
        prior = ('Started validating: ' + self.workspace + '/vst3/CS-80 Reference.vst3\n'
                 'Starting tests in: fixture\nCompleted tests in: fixture\n').encode()
        data[name] = prior
        data[lane['core']] = core(200, 139) + core(201, 0)
        step = self.step(lane)
        output = m.lines(prior) + ['##[warning]' + m.retry_warning(2)] + m.lines(data[lane['console']])
        step['group']['runtime'] = [dict(timestamp=None, message=line) for line in output]
        result = m.lane_binding(lane, data, step, self.workspace)
        self.assertEqual((result['invocations'], result['retries']), (2, 1))
        for mutation in ('warning', 'colon', 'gap', 'core-count'):
            dd, ss = dict(data), copy.deepcopy(step)
            if mutation == 'warning':
                ss['group']['runtime'].pop(len(m.lines(prior)))
            elif mutation == 'colon':
                dd[name] = prior.replace(b'Completed tests in:', b'Completed tests in')
                ss['group']['runtime'][2]['message'] = 'Completed tests in fixture'
            elif mutation == 'gap':
                dd[name.replace('attempt1', 'attempt2')] = dd.pop(name)
            else:
                dd[lane['core']] = core(201, 0)
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                m.lane_binding(lane, dd, ss, self.workspace)

    def test_05_post_success_crash_and_exact_binding(self):
        lane = CONTRACT['lanes'][0]
        for mutation in ('post-success-crash', 'report', 'path', 'runtime-extra', 'raw-normalized'):
            data, step = dict(self.members), self.step(lane)
            if mutation == 'post-success-crash':
                data[lane['core']] = core(202, -11)
            elif mutation == 'report':
                data[lane['report']] += b'EXTRA\n'
            elif mutation == 'path':
                data[lane['console']] = data[lane['console']].replace(b'/vst3/', b'/different/')
            elif mutation == 'runtime-extra':
                step['group']['runtime'].append(dict(timestamp=None, message='another invocation'))
            else:
                data[lane['core']] = core(202, 0).replace(b'exit: 0', b'exit: 139')
            with self.subTest(mutation=mutation), self.assertRaises(ValueError):
                m.lane_binding(lane, data, step, self.workspace)

    def test_06_gui_completed_body_required(self):
        lane = CONTRACT['lanes'][1]
        data, step = dict(self.members), self.step(lane)
        marker = b'Completed tests in pluginval / Editor Automation\n'
        for key in ('console', 'report'):
            data[lane[key]] = data[lane[key]].replace(marker, b'')
        step['group']['runtime'] = [r for r in step['group']['runtime'] if r['message'] != marker.decode().strip()]
        with self.assertRaises(ValueError):
            m.lane_binding(lane, data, step, self.workspace)


if __name__ == '__main__':
    unittest.main(verbosity=2)
