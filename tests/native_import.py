#!/usr/bin/env python3
"""Development tests for the native helper; Python is not shipped to handhelds."""
from pathlib import Path
import hashlib, os, signal, subprocess, tempfile, time, unittest, fcntl
ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build/native-import/prepare-game-test'
PROD=ROOT/'build/native-import/prepare-game-host'
INSTALLER='setup_conflict_-_desert_storm_1.0_(21657).exe'
class NativeImport(unittest.TestCase):
 def setUp(self):
  self.tmp=tempfile.TemporaryDirectory();self.addCleanup(self.tmp.cleanup);self.root=Path(self.tmp.name)
  self.installer=self.root/INSTALLER;self.installer.write_bytes(b'test installer')
  self.payload=b'fixture';self.manifest=self.root/'game-files.sha256'
  self.manifest.write_text(hashlib.sha256(self.payload).hexdigest()+'  test.dat\n')
  self.extractor=self.root/'extractor';self.extractor.write_text('#!/bin/sh\nwhile [ "$1" != --output-dir ]; do shift; done\nshift\nprintf fixture > "$1/test.dat"\n');self.extractor.chmod(0o755)
  self.env=dict(os.environ,DS_TEST_INSTALLER_SHA=hashlib.sha256(self.installer.read_bytes()).hexdigest(),DS_TEST_FREE_BYTES=str(2**40))
 def run_helper(self,*args,code=0,prod=False):
  p=subprocess.run([str(PROD if prod else BIN),'--root',str(self.root),'--extractor',str(self.extractor),*args],env=self.env,capture_output=True,text=True,timeout=15)
  self.assertEqual(p.returncode,code,p.stderr);return p
 def test_import_cached_and_old_marker_migration(self):
  self.run_helper(code=10);self.assertTrue(self.installer.exists());self.assertEqual((self.root/'data/test.dat').read_bytes(),self.payload)
  marker=self.root/'data-ready.native';stamp=marker.stat().st_mtime_ns
  self.run_helper(prod=True);self.assertEqual(stamp,marker.stat().st_mtime_ns)
  marker.unlink();(self.root/'data-ready.json').write_text('{}');self.run_helper(prod=True);self.assertTrue(marker.exists());self.assertFalse(list(self.root.glob('.import-*')))
 def test_wrong_installer_and_production_ignores_test_override(self):
  self.run_helper(prod=True,code=1);self.assertTrue(self.installer.exists());self.installer.write_bytes(b'bad');self.run_helper(code=1);self.assertFalse((self.root/'data').exists())
 def test_partial_data_preserved(self):
  (self.root/'data').mkdir();(self.root/'data/save').write_text('keep');self.run_helper(code=1);self.assertEqual((self.root/'data/save').read_text(),'keep')
 def test_changed_data_even_with_preserved_mtime(self):
  self.run_helper(code=10);p=self.root/'data/test.dat';st=p.stat();p.write_bytes(b'corrupt');os.utime(p,ns=(st.st_atime_ns,st.st_mtime_ns));self.run_helper(code=1);self.assertEqual(p.read_bytes(),b'corrupt')
 def test_portmaster_library_directory(self):
  (self.root/"libs.aarch64").mkdir()
  self.extractor.write_text(self.extractor.read_text().replace("#!/bin/sh\n", "#!/bin/sh\ncase \"$LD_LIBRARY_PATH\" in \""+str(self.root/"libs.aarch64")+"\"*) ;; *) exit 7 ;; esac\n"))
  self.run_helper(code=10)
 def test_failure_cleanup(self):
  self.extractor.write_text('#!/bin/sh\nexit 2\n');self.run_helper(code=1);self.assertFalse(list(self.root.glob('.import-*')));self.assertTrue(self.installer.exists())
 def test_cancellation_reaps_extractor(self):
  self.extractor.write_text('#!/bin/sh\necho $$ > "'+str(self.root/'child.pid')+'"\nexec sleep 60\n')
  p=subprocess.Popen([str(BIN),'--root',str(self.root),'--extractor',str(self.extractor)],env=self.env)
  try:
   deadline=time.monotonic()+5
   while not (self.root/'child.pid').exists() and time.monotonic()<deadline:time.sleep(.01)
   self.assertTrue((self.root/'child.pid').exists());child=int((self.root/'child.pid').read_text());p.send_signal(signal.SIGTERM);self.assertEqual(p.wait(timeout=7),130)
   self.assertFalse(Path('/proc',str(child)).exists());self.assertFalse(list(self.root.glob('.import-*')));self.assertTrue(self.installer.exists())
  finally:
   if p.poll() is None:p.kill();p.wait()
 def test_delete_guards_and_saves(self):
  self.run_helper('--delete-installer',code=1);self.run_helper(code=10);(self.root/'save').write_text('keep');self.installer.write_bytes(b'bad');self.run_helper('--delete-installer',code=1);self.assertTrue(self.installer.exists())
  self.installer.write_bytes(b'test installer');self.run_helper('--delete-installer');self.assertFalse(self.installer.exists());self.assertEqual((self.root/'save').read_text(),'keep');self.assertTrue((self.root/'data/test.dat').exists())
 def test_space_guard(self):
  self.env['DS_TEST_FREE_BYTES']='0';self.run_helper(code=1);self.assertFalse(list(self.root.glob('.import-*')))
 def test_invalid_manifests(self):
  sha=hashlib.sha256(self.payload).hexdigest()
  for value in ['',sha+'  ../escape\n',sha+'  /escape\n',sha+'  a/./b\n',sha+'  test.dat\n'+sha+'  TEST.dat\n']:
   self.manifest.write_text(value);self.run_helper(code=1)
 def test_data_symlink_rejected(self):
  source=self.root/'elsewhere';source.mkdir();(source/'test.dat').write_bytes(self.payload);(self.root/'data').symlink_to(source);self.run_helper(code=1);self.assertTrue((source/'test.dat').exists())
 def test_file_symlink_rejected(self):
  (self.root/'data').mkdir();(self.root/'original').write_bytes(self.payload);(self.root/'data/test.dat').symlink_to(self.root/'original');self.run_helper(code=1)
 def test_fifo_rejected_without_hanging(self):
  (self.root/"data").mkdir();os.mkfifo(self.root/"data/test.dat");self.run_helper(code=1)
 def test_lock_preserves_other_status(self):
  (self.root/'install-status.txt').write_text('keep');lock=open(self.root/'.install.lock','w');self.addCleanup(lock.close);fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB);self.run_helper(code=1);self.assertEqual((self.root/'install-status.txt').read_text(),'keep')
 def test_manual_casefold_import_and_check(self):
  source=self.root/'source';source.mkdir();(source/'TEST.DAT').write_bytes(self.payload);(source/'save').write_text('not copied')
  self.run_helper('--source',str(source),'--check-only',prod=True);self.assertFalse((self.root/'data').exists())
  self.run_helper('--source',str(source),prod=True);self.assertEqual((self.root/'data/test.dat').read_bytes(),self.payload);self.assertFalse((self.root/'data/save').exists());self.assertTrue((source/'save').exists())
 def test_destination_race_preserved(self):
  self.extractor.write_text('#!/bin/sh\nwhile [ "$1" != --output-dir ]; do shift; done\nshift\nprintf fixture > "$1/test.dat"\nmkdir "'+str(self.root/'data')+'"\nprintf keep > "'+str(self.root/'data/player-save')+'"\n')
  self.run_helper(code=1);self.assertEqual((self.root/'data/player-save').read_text(),'keep');self.assertFalse(list(self.root.glob('.import-*')))
 def test_streaming_hash_vectors(self):
  data=self.root/'data';data.mkdir();(data/'test.dat').write_bytes(b'')
  for payload in [b'',b'abc',b'a'*55,b'a'*56,b'a'*63,b'a'*64,b'a'*65,b'a'*1000000]:
   (data/'test.dat').write_bytes(payload);self.manifest.write_text(hashlib.sha256(payload).hexdigest()+'  test.dat\n');self.run_helper(prod=True)
if __name__=='__main__':unittest.main()
