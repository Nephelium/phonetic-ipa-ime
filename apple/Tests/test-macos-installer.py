"""Authorized tests of this product's own installer; never changes developer tools."""
from pathlib import Path
import hashlib
import json
import os
import plistlib
import signal
import subprocess
import sys
import time

root = Path(__file__).resolve().parents[1]
installer = Path(sys.argv[1]).resolve()
resources = installer / 'Contents/Resources'
target = Path.home() / 'Library/Input Methods/PhoneticIPA.app'
qa = root / ('build/installer-qa-' + time.strftime('%Y%m%d-%H%M%S'))
qa.mkdir(exist_ok=False)
assert not target.is_symlink() and target.is_dir()
original_inode = target.stat().st_ino
identity = lambda p: plistlib.loads((p / 'Contents/Info.plist').read_bytes())['CFBundleIdentifier']
assert identity(target) == 'org.nephelium.inputmethod.PhoneticIPA'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
original = {p.relative_to(target).as_posix(): sha(p) for p in target.rglob('*') if p.is_file()}
source = resources / 'Payload/PhoneticIPA.app'
expected = {p.relative_to(source).as_posix(): sha(p) for p in source.rglob('*') if p.is_file()}
backup_root = Path.home() / 'Library/Application Support/PhoneticIPA/Install Backups'
existing_backups = set(backup_root.glob('*'))
preferences = Path.home() / 'Library/Preferences/com.apple.HIToolbox.plist'
def foreign_sources():
    entries = plistlib.loads(preferences.read_bytes())['AppleEnabledInputSources']
    return [entry for entry in entries if entry.get('Bundle ID') != 'org.nephelium.inputmethod.PhoneticIPA']
original_foreign = foreign_sources()
script = resources / 'Scripts/install-macos.sh'
text = script.read_text()
assert all(word not in text for word in ['xcrun', 'clang ', 'swiftc', 'xcode-select', 'DEVELOPER_DIR'])
env = dict(os.environ, IPA_MAC_APP=str(resources / 'Payload/PhoneticIPA.app'), IPA_REGISTRATION_APP=str(resources / 'Registration/RegisterInputSource.app'), DEVELOPER_DIR='/__ipa_qa_no_developer_tools__', PATH='/usr/bin:/bin:/usr/sbin:/sbin')
backup = qa / 'previous-original.bundle'
guard = root / 'build/InstallerSelectionGuard'
state = qa / 'previous-session.json'

def run(args, name, expected=0, environment=env):
    with (qa / name).open('wb') as log:
        result = subprocess.run(args, env=environment, stdout=log, stderr=subprocess.STDOUT)
    assert result.returncode == expected, (name, result.returncode)

# A missing helper must fail before changing the installed product.
invalid = dict(env, IPA_REGISTRATION_APP=str(qa / 'missing-helper.app'))
run(['/bin/bash', str(script), '--update', '--no-open'], 'missing-helper.log', expected=1, environment=invalid)
assert original == {p.relative_to(target).as_posix(): sha(p) for p in target.rglob('*') if p.is_file()}
run([str(guard), 'begin', str(state)], 'guard-begin.log')
try:
    # This build account has Xcode products and earlier installer test copies.
    # They share our ID and can win LaunchServices resolution over the installed
    # app. Unregister only this project's known build copies, retaining all files.
    candidates = [root / 'build/CustomIcon-Signature-QA.app', root / 'build/Debug/PhoneticIPA-Mac.app', root / 'build/products/Debug/PhoneticIPA-Mac.app']
    candidates += list((root / 'build').glob('installer-*/*.app/Contents/Resources/Payload/PhoneticIPA.app'))
    lsregister = '/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister'
    for copy in candidates:
        if copy.is_dir():
            assert identity(copy) == identity(target)
            result = subprocess.run([lsregister, '-u', str(copy)], capture_output=True)
            assert result.returncode == 0 or b'-10814' in result.stdout + result.stderr
    executable = str(target / 'Contents/MacOS/PhoneticIPA-Mac')
    for line in subprocess.check_output(['/bin/ps', '-axo', 'pid=,args='], text=True).splitlines():
        fields = line.strip().split(None, 1)
        if len(fields) == 2 and (fields[1] == executable or fields[1].startswith(executable + ' ')):
            pid = int(fields[0]); os.kill(pid, signal.SIGTERM)
            for attempt in range(50):
                try: os.kill(pid, 0)
                except ProcessLookupError: break
                time.sleep(.1)
            else: raise RuntimeError('Own input method did not stop; no files moved')
    target.rename(backup)
    try:
        assert not target.exists()
        run(['/bin/bash', '-x', str(script), '--repair-enabled-list', '--no-open'], 'first-install-no-tools.log')
        assert identity(target) == identity(backup)
        assert expected == {p.relative_to(target).as_posix(): sha(p) for p in target.rglob('*') if p.is_file()}
        subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(target)], check=True)
        assert b'registration-complete=1' in (qa / 'first-install-no-tools.log').read_bytes()
    finally:
        if target.exists(): target.rename(qa / 'first-installed.bundle')
        backup.rename(target)
    assert target.stat().st_ino == original_inode
    # Real production button and backend, with an invalid developer directory.
    run([str(root / 'build/InstallerUITests'), str(resources), str(qa / 'installer-complete.png')], 'native-ui-upgrade.log')
    assert target.stat().st_ino == original_inode
    assert expected == {p.relative_to(target).as_posix(): sha(p) for p in target.rglob('*') if p.is_file()}
    preserved = False
    for directory in set(backup_root.glob('*')) - existing_backups:
        previous = directory / 'previous.bundle'
        if previous.exists() and original == {p.relative_to(previous).as_posix(): sha(p) for p in previous.rglob('*') if p.is_file()}:
            preserved = True
    assert preserved, 'Upgrade must preserve the previous app in its backup'
    assert foreign_sources() == original_foreign, 'Other input sources, ordering and metadata must be unchanged'
finally:
    if backup.exists():
        if target.exists(): target.rename(qa / 'failed-test.bundle')
        backup.rename(target)
    subprocess.run(['/usr/bin/open', '-g', str(target)], check=True)
    run([str(guard), 'restore', str(state)], 'guard-restore.log')
record = {'passed': True, 'first_install_target_absent': True, 'same_account_previously_registered': True, 'missing_helper_rejected_before_mutation': True, 'native_ui_actual_upgrade': True, 'original_bundle_directory_preserved': True, 'invalid_developer_directory_during_runtime': True, 'new_payload_files_verified': True, 'previous_app_backup_verified': True, 'qa': str(qa)}
(qa / 'result.json').write_text(json.dumps(record, indent=2))
print(json.dumps(record))
