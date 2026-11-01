"""Propagate msime-windows/version.txt to every place that repeats the product version.

    python scripts/sync-version.py           # rewrite the copies
    python scripts/sync-version.py --check   # exit 1 when any copy differs

The Android versionCode and Apple CURRENT_PROJECT_VERSION are build numbers that
must only grow; pass --build N to set both, otherwise they are left unchanged.
Line endings and encodings of the edited files are preserved.
"""
import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
# Two layouts: the local workspace (msime-windows/ beside scripts/) and the public repository
# (repository root holds version.txt; these scripts live in prototype/scripts/).
if (ROOT / 'msime-windows').is_dir():
    UPSTREAM, PROJECT, SCRIPTS = ROOT / 'msime-windows', ROOT, 'scripts'
else:
    UPSTREAM = PROJECT = ROOT.parent
    SCRIPTS = 'prototype/scripts'
VERSION_FILE = UPSTREAM / 'version.txt'

parser = argparse.ArgumentParser()
parser.add_argument('--check', action='store_true')
parser.add_argument('--build', type=int)
args = parser.parse_args()

version = VERSION_FILE.read_text(encoding='utf-8').strip()
if not re.fullmatch(r'\d+\.\d+\.\d+', version):
    raise SystemExit(f'{VERSION_FILE} must hold MAJOR.MINOR.PATCH, got {version!r}')

# (file, pattern with one group for the prefix and one for the suffix, replacement value)
targets = [
    (UPSTREAM / 'engine/ipa/update_manifest.h', r'(inline constexpr char kVersion\[\] = ")[^"]*(";)', version),
    (PROJECT / 'android/app/build.gradle.kts', r'(versionName = ")[^"]*(")', version),
    (PROJECT / 'apple/scripts/project.py', r"('MARKETING_VERSION':')[^']*(')", version),
    (PROJECT / 'apple/Shared/Updates.swift', r'(as\? String \?\? ")[^"]*(")', version),
    (PROJECT / SCRIPTS / 'Windows-Installation.ps1', r"(\$script:IpaVersion = ')[^']*(')", version),
]
if args.build is not None:
    targets += [
        (PROJECT / 'android/app/build.gradle.kts', r'(versionCode = )\d+()', str(args.build)),
        (PROJECT / 'apple/scripts/project.py', r"('CURRENT_PROJECT_VERSION':')[^']*(')", str(args.build)),
    ]

stale = []
for path, pattern, value in targets:
    relative = path.relative_to(PROJECT).as_posix()
    raw = path.read_bytes()
    bom = raw.startswith(b'\xef\xbb\xbf')
    text = raw[3 if bom else 0:].decode('utf-8')
    updated, count = re.subn(pattern, lambda m: m.group(1) + value + m.group(2), text)
    if count != 1:
        raise SystemExit(f'{relative}: expected one version field, found {count}')
    if updated != text:
        stale.append(relative)
        if not args.check:
            path.write_bytes((b'\xef\xbb\xbf' if bom else b'') + updated.encode('utf-8'))

# The TSF version resource has its own upstream stamping script.
stamp = [sys.executable, str(UPSTREAM / 'scripts' / 'apply_version.py')]
if args.check:
    stamp.append('--check')
if subprocess.run(stamp).returncode != 0:
    stale.append('windows/src/IME/MetasequoiaIME.rc')

if args.check and stale:
    print('Version copies differ from version.txt (' + version + '):\n  ' + '\n  '.join(stale))
    sys.exit(1)
print(('Checked' if args.check else 'Synchronized') + ' version ' + version)
