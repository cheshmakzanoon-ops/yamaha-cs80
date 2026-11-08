#!/usr/bin/env python3
"""Run only the offline core renderer; no plugin, GUI or validator."""
import argparse
import os
import signal
import sys
from pathlib import Path
import subprocess
import tempfile
import unittest
import wave

parser = argparse.ArgumentParser()
parser.add_argument('--render', required=True)
args, remaining = parser.parse_known_args()
RENDER = str(Path(args.render).resolve())


class RenderArguments(unittest.TestCase):
    def test_rejected_before_output(self):
        cases = [['--unknown'], ['--sr'], ['--seconds'], ['--out'], ['--out', '']]
        for option in ('--sr', '--seconds'):
            for value in ('', 'abc', 'nan', 'inf', '-inf', '1e999', '1e-999', '48000garbage', '-1', '0'):
                cases.append([option, value])
        cases += [['--sr', x] for x in ('7999', '768001', '48000.5')]
        cases += [['--seconds', x] for x in ('1e50', '0.0000001', '100000')]
        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'must-not-exist.wav'
            for case in cases:
                with self.subTest(arguments=case):
                    result = subprocess.run([RENDER, '--out', str(output), *case],
                                            capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 2, result.stderr)
                    self.assertFalse(output.exists())

    def test_supported_rates_match_wav(self):
        with tempfile.TemporaryDirectory() as tmp:
            for rate in (8000, 48000, 768000):
                with self.subTest(rate=rate):
                    output = Path(tmp) / f'{rate}.wav'
                    result = subprocess.run([RENDER, '--sr', str(rate), '--seconds', '0.001',
                                             '--out', str(output)], capture_output=True, timeout=10)
                    self.assertEqual(result.returncode, 0, result.stderr)
                    with wave.open(str(output)) as wav:
                        self.assertEqual((wav.getframerate(), wav.getnframes(), wav.getnchannels(),
                                          wav.getsampwidth()), (rate, rate // 1000, 2, 2))

    @unittest.skipUnless(os.name == 'posix' and hasattr(signal, 'SIGXFSZ'),
                         'requires POSIX child-only file size limit')
    def test_buffered_write_failure_is_reported(self):
        import resource

        def limit_output():
            signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
            resource.setrlimit(resource.RLIMIT_FSIZE, (64, 64))

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'truncated.wav'
            # A 48-frame stereo PCM16 WAV needs 236 bytes. The stdio buffer
            # accepts it, but flushing at fclose must fail at the 64-byte limit.
            result = subprocess.run([RENDER, '--seconds', '0.001', '--out', str(output)],
                                    capture_output=True, timeout=10, preexec_fn=limit_output)
            self.assertEqual(result.returncode, 1, result.stdout)
            self.assertIn(b'Could not write', result.stderr)
            self.assertNotIn(b'Wrote ', result.stdout)
            self.assertEqual(output.stat().st_size, 64)

    @unittest.skipUnless(os.name == 'posix' and hasattr(signal, 'SIGXFSZ'),
                         'requires POSIX child-only resource limits')
    def test_long_render_has_bounded_buffers(self):
        import resource
        if not hasattr(resource, 'RLIMIT_AS'):
            self.skipTest('address-space limit unavailable')

        # Run setup in a visible, isolated interpreter rather than preexec_fn:
        # macOS may expose RLIMIT_AS yet reject setrlimit with ValueError.
        # Only that specific unavailable capability skips this memory test.
        launcher = """
import os, resource, signal, sys
try:
    resource.setrlimit(resource.RLIMIT_AS, (256 * 1024 * 1024,) * 2)
except (ValueError, OSError) as error:
    print('RLIMIT_AS unavailable: ' + str(error), file=sys.stderr)
    sys.exit(77)
signal.signal(signal.SIGXFSZ, signal.SIG_IGN)
resource.setrlimit(resource.RLIMIT_FSIZE, (64, 64))
resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
os.execv(sys.argv[1], sys.argv[1:])
"""

        with tempfile.TemporaryDirectory() as tmp:
            output = Path(tmp) / 'long.wav'
            # Accepted duration formerly allocated 384 MB before rendering.
            # Bounded buffers reach the deliberate disk limit instead. This
            # does not spend 1000 seconds rendering or claim a full long WAV.
            result = subprocess.run([sys.executable, '-c', launcher, RENDER,
                                     '--seconds', '1000', '--out', str(output)],
                                    capture_output=True, timeout=10)
            if result.returncode == 77 and result.stderr.startswith(b'RLIMIT_AS unavailable: '):
                self.assertFalse(output.exists())
                self.skipTest(result.stderr.decode(errors='replace').strip())
            self.assertEqual(result.returncode, 1, result.stderr)
            self.assertIn(b'Could not write', result.stderr)
            self.assertNotIn(b'Wrote ', result.stdout)
            self.assertEqual(output.stat().st_size, 64)

    def test_help(self):
        result = subprocess.run([RENDER, '--help'], capture_output=True, timeout=10)
        self.assertEqual(result.returncode, 0)
        self.assertIn(b'Usage:', result.stderr)


if __name__ == '__main__':
    unittest.main(argv=['test_render_cli.py', *remaining])
