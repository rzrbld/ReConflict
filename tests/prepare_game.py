#!/usr/bin/env python3
"""First-run safety tests use synthetic files, never the player's installation."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('prepare', Path(__file__).resolve().parents[1] / 'scripts/prepare_game.py')
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

class ImportTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.installer = self.root / m.INSTALLER
        self.installer.write_bytes(b'SYNTHETIC TEST INSTALLER')
        self.payload = b'fixture'
        self.sha = m.hashlib.sha256(self.payload).hexdigest()
        (self.root / 'game-files.sha256').write_text(self.sha + '  test.dat\n')
        self.extractor = self.root / 'extractor'
        self.extractor.write_text('#!/usr/bin/env python3\nimport sys\nfrom pathlib import Path\np=Path(sys.argv[sys.argv.index("--output-dir")+1]);(p/"test.dat").write_bytes(b"fixture")\n')
        self.extractor.chmod(0o755)
        p = patch.object(m, 'INSTALLER_SHA', m.digest(self.installer))
        p.start(); self.addCleanup(p.stop)
        p = patch.object(m.shutil, 'disk_usage', return_value=type('Space', (), {'free': 2**40})())
        p.start(); self.addCleanup(p.stop)

    def install(self):
        return m.prepare(self.root, self.extractor)

    def test_install_and_cached_relaunch(self):
        self.assertEqual(self.install(), 10)
        self.assertTrue(self.installer.exists())
        self.assertEqual((self.root / 'data/test.dat').read_bytes(), self.payload)
        with patch.object(m, 'validate', side_effect=AssertionError('rehashed unchanged data')):
            self.assertEqual(self.install(), 0)
        self.assertFalse(list(self.root.glob('.import-*')))

    def test_wrong_installer_kept(self):
        self.installer.write_bytes(b'wrong')
        with self.assertRaisesRegex(ValueError, 'UNSUPPORTED'): self.install()
        self.assertEqual(self.installer.read_bytes(), b'wrong')
        self.assertFalse((self.root / 'data').exists())

    def test_existing_partial_data_never_overwritten(self):
        (self.root / 'data').mkdir()
        (self.root / 'data/player-save').write_bytes(b'KEEP')
        with self.assertRaisesRegex(ValueError, 'Missing'): self.install()
        self.assertEqual((self.root / 'data/player-save').read_bytes(), b'KEEP')
        self.assertTrue(self.installer.exists())

    def test_changed_data_revalidated_and_preserved(self):
        self.install()
        (self.root / 'data/test.dat').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'Wrong version'): self.install()
        self.assertEqual((self.root / 'data/test.dat').read_bytes(), b'changed')

    def test_extraction_failure_cleanup(self):
        self.extractor.write_text('#!/bin/sh\nexit 2\n')
        with self.assertRaisesRegex(ValueError, 'EXTRACTION FAILED'): self.install()
        self.assertFalse(list(self.root.glob('.import-*')))
        self.assertFalse((self.root / 'data').exists())
        self.assertTrue(self.installer.exists())

    def test_cancellation_cleans_stage_and_waits_for_child(self):
        self.extractor.write_text('#!/usr/bin/env python3\nimport time\ntime.sleep(60)\n')
        original = m.subprocess.Popen.wait
        def interrupted(child, timeout=None):
            if timeout == 0.5: raise m.Cancelled()
            return original(child, timeout=timeout)
        with patch.object(m.subprocess.Popen, 'wait', interrupted):
            with self.assertRaises(m.Cancelled): self.install()
        self.assertFalse(list(self.root.glob('.import-*')))
        self.assertTrue(self.installer.exists())

    def test_delete_requires_valid_data_and_unchanged_installer(self):
        with self.assertRaises(ValueError): m.delete_installer(self.root)
        self.install()
        self.installer.write_bytes(b'new installer')
        with self.assertRaisesRegex(ValueError, 'INSTALLER CHANGED'): m.delete_installer(self.root)
        self.assertTrue(self.installer.exists())

    def test_explicit_delete_retains_data_and_saves(self):
        self.install()
        (self.root / 'save').write_bytes(b'keep')
        self.assertEqual(m.delete_installer(self.root), 0)
        self.assertFalse(self.installer.exists())
        self.assertEqual((self.root / 'data/test.dat').read_bytes(), self.payload)
        self.assertEqual((self.root / 'save').read_bytes(), b'keep')

    def test_space_and_manifest_guards(self):
        with patch.object(m.shutil, 'disk_usage', return_value=type('Space', (), {'free': 0})()):
            with self.assertRaisesRegex(ValueError, 'FREE SPACE'): self.install()
        for text in ['', self.sha + '  ../escape\n', self.sha + '  /escape\n']:
            (self.root / 'game-files.sha256').write_text(text)
            with self.assertRaises(ValueError): self.install()

if __name__ == '__main__': unittest.main()
