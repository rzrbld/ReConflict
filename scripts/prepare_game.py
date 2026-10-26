#!/usr/bin/env python3
"""Transactional first-run import, invoked by the SDL launcher beside the game.

Exit 0: existing data ready; 10: newly imported (offer installer deletion);
1: error; 130: cancelled. Never executes the Windows installer.
"""
import argparse
import fcntl
import hashlib
import json
import os
from pathlib import Path
import resource
import shutil
import signal
import subprocess
import tempfile
import time

INSTALLER = 'setup_conflict_-_desert_storm_1.0_(21657).exe'
INSTALLER_SHA = 'f840e10a7da55a71e89e5d83815c3f3e2bdec1206b20471291d87a3a02ea81aa'

class Cancelled(Exception):
    pass

def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()

def atomic_text(path, text):
    fd, name = tempfile.mkstemp(prefix=path.name + '.', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as f:
            f.write(text)
            f.flush()
            os.fsync(f.fileno())
        os.replace(name, path)
    finally:
        if os.path.exists(name):
            os.unlink(name)

def status(root, percent, message):
    atomic_text(root / 'install-status.txt', f'{percent}\n{message}\n')

def manifest_entries(path):
    entries = []
    for line in path.read_text().splitlines():
        sha, name = line.split('  ', 1)
        p = Path(name)
        if (len(sha) != 64 or any(c not in '0123456789abcdef' for c in sha)
                or not name or p.is_absolute() or '..' in p.parts):
            raise ValueError('Invalid game manifest')
        entries.append((sha, name))
    if not entries or len({n.casefold() for _, n in entries}) != len(entries):
        raise ValueError('Empty or ambiguous game manifest')
    return entries

def file_state(data, entries):
    result = []
    if data.is_symlink():
        raise ValueError('Data folder must not be a symlink')
    for _, name in entries:
        path = data / name
        if not path.resolve().is_relative_to(data.resolve()) or not path.is_file():
            raise ValueError('Missing game file: ' + name)
        st = path.stat()
        result.append([name, st.st_size, st.st_mtime_ns])
    return result

def validate(root, data, entries, durable=False):
    file_state(data, entries)
    for i, (sha, name) in enumerate(entries):
        status(root, int(100 * i / len(entries)), 'VERIFYING ' + name)
        if digest(data / name) != sha:
            raise ValueError('Wrong version or damaged game file: ' + name)
        if durable:
            # Do not publish a ready marker while newly extracted contents
            # exist only in the kernel's write-back cache.
            with (data / name).open('rb') as f:
                os.fsync(f.fileno())

def ready(root, entries):
    marker = root / 'data-ready.json'
    expected = {'manifest': digest(root / 'game-files.sha256'),
                'files': file_state(root / 'data', entries)}
    try:
        if json.loads(marker.read_text()) == expected:
            return
    except (OSError, ValueError):
        pass
    validate(root, root / 'data', entries)
    atomic_text(marker, json.dumps(expected))

def prepare(root, extractor=None):
    entries = manifest_entries(root / 'game-files.sha256')
    data = root / 'data'
    if data.exists() or data.is_symlink():
        if data.is_dir() and not data.is_symlink() and not any(data.iterdir()):
            data.rmdir()
        else:
            ready(root, entries)
            status(root, 100, 'GAME FILES READY')
            return 0
    installer = root / INSTALLER
    if not installer.is_file():
        raise ValueError('COPY YOUR GOG INSTALLER HERE. SEE README.')
    status(root, -1, 'CHECKING GOG INSTALLER')
    if installer.is_symlink() or digest(installer) != INSTALLER_SHA:
        raise ValueError('UNSUPPORTED INSTALLER. NEED GOG 21657 ENGLISH.')
    # Selected game files occupy 967 MB. Reserve space for verification/logs.
    if shutil.disk_usage(root).free < 1100 * 1024 * 1024:
        raise ValueError('NEED AT LEAST 1.1 GIB FREE SPACE TO IMPORT.')
    extractor = Path(extractor) if extractor else root / 'tools/innoextract'
    if not extractor.is_file():
        raise ValueError('INNOEXTRACT MISSING. REINSTALL PORT PACKAGE.')
    stage = Path(tempfile.mkdtemp(prefix='.import-', dir=root))
    child = None
    started = time.monotonic()
    try:
        args = [str(extractor), '--extract', '--exclude-temp', '--color=0',
                '--progress=0', '--language', 'en-US', '--output-dir', str(stage)]
        for _, name in entries:
            args += ['--include', name]
        args.append(str(installer))
        env = os.environ.copy()
        private_libs = extractor.parent / 'lib'
        if private_libs.is_dir():
            env['LD_LIBRARY_PATH'] = str(private_libs) + (':' + env['LD_LIBRARY_PATH'] if env.get('LD_LIBRARY_PATH') else '')
        with (root / 'install.log').open('a') as log:
            child = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT, env=env)
            while child.poll() is None:
                count = sum(1 for p in stage.rglob('*') if p.is_file())
                status(root, min(99, int(count * 100 / len(entries))),
                       f'EXTRACTING FILES {count} OF {len(entries)}')
                try:
                    child.wait(timeout=0.5)
                except subprocess.TimeoutExpired:
                    pass
            if child.returncode:
                raise ValueError('EXTRACTION FAILED. SEE INSTALL.LOG.')
        validate(root, stage, entries, durable=True)
        if data.exists() or data.is_symlink():
            raise ValueError('DATA FOLDER CHANGED. EXISTING FILES KEPT.')
        os.rename(stage, data)
        atomic_text(root / 'data-ready.json', json.dumps({
            'manifest': digest(root / 'game-files.sha256'),
            'files': file_state(data, entries)}))
        atomic_text(root / 'install-result.json', json.dumps({
            'seconds': round(time.monotonic() - started, 3),
            'extractor_peak_rss_kib': resource.getrusage(resource.RUSAGE_CHILDREN).ru_maxrss,
            'files_verified': len(entries), 'installer_sha256': INSTALLER_SHA}, indent=2))
        status(root, 100, 'IMPORT COMPLETE. INSTALLER STILL KEPT.')
        return 10
    finally:
        if child and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
        if stage.exists():
            shutil.rmtree(stage)

def delete_installer(root):
    ready(root, manifest_entries(root / 'game-files.sha256'))
    installer = root / INSTALLER
    status(root, -1, 'CHECKING INSTALLER BEFORE DELETION')
    if installer.is_symlink() or digest(installer) != INSTALLER_SHA:
        raise ValueError('INSTALLER CHANGED. FILE KEPT.')
    installer.unlink()
    status(root, 100, 'INSTALLER DELETED. GAME DATA KEPT.')
    return 0

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parent)
    p.add_argument('--delete-installer', action='store_true', help='Only after explicit user choice')
    p.add_argument('--extractor', type=Path, help='Development: native host extractor')
    a = p.parse_args()
    root = a.root.resolve(strict=True)
    with (root / '.install.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return 1  # Do not overwrite another launcher's progress.
        def cancel(*_):
            raise Cancelled()
        signal.signal(signal.SIGTERM, cancel)
        signal.signal(signal.SIGINT, cancel)
        try:
            return delete_installer(root) if a.delete_installer else prepare(root, a.extractor)
        except Cancelled:
            status(root, 0, 'CANCELLED. ORIGINAL INSTALLER KEPT.')
            return 130
        except Exception as e:
            status(root, 0, str(e).replace('\n', ' ')[:180])
            with (root / 'install.log').open('a') as log:
                log.write(f'{type(e).__name__}: {e}\n')
            return 1

if __name__ == '__main__':
    raise SystemExit(main())
