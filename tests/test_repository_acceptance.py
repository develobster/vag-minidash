"""Acceptance runner fault contracts; no physical transport or device access."""
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from tools.verify_repository import main, run


class RepositoryAcceptanceTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def test_success_is_logged(self):
        result = run([sys.executable, '-c', "print('fixture')"], self.root, 'success')
        self.assertEqual(result['status'], 'PASS')
        self.assertIn(b'fixture', (self.root / 'success.log').read_bytes())

    def test_nonzero_command_fails(self):
        result = run([sys.executable, '-c', 'raise SystemExit(7)'], self.root, 'failure')
        self.assertEqual(result['exit_code'], 7)
        self.assertEqual(result['status'], 'FAIL')

    def test_missing_executable_fails(self):
        result = run([str(self.root / 'missing-program')], self.root, 'missing')
        self.assertEqual(result['status'], 'FAIL')
        self.assertIn('error', result)

    def test_existing_log_is_preserved(self):
        path = self.root / 'existing.log'
        path.write_bytes(b'original')
        result = run([sys.executable, '-c', 'print(123)'], self.root, 'existing')
        self.assertEqual(result['status'], 'FAIL')
        self.assertEqual(path.read_bytes(), b'original')

    def test_missing_native_gate_cannot_pass(self):
        output = self.root / 'output'
        with patch('tools.verify_repository.run', return_value={'status': 'PASS'}), \
                patch('tools.verify_repository.importlib.util.find_spec', return_value=object()):
            self.assertEqual(main(['--output-dir', str(output)]), 1)
        summary = json.loads((output / 'summary.json').read_text(encoding='utf-8'))
        self.assertEqual(summary['status'], 'INCOMPLETE_OR_FAILED')
        self.assertEqual(summary['gates']['web']['status'], 'NOT_RUN')
        self.assertFalse(summary['hardware_validation'])
        self.assertFalse(summary['vehicle_validation'])
        before = (output / 'summary.json').read_bytes()
        with self.assertRaises(FileExistsError):
            main(['--output-dir', str(output)])
        self.assertEqual((output / 'summary.json').read_bytes(), before)
