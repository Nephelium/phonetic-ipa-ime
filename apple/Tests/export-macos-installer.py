"""Export changed installer files plus a verified manifest, without retransmitting the fixed payload."""
from pathlib import Path
import hashlib
import json
import os
import plistlib
import shutil
import subprocess
import sys
import zipfile

root = Path(__file__).resolve().parents[1]
app = Path(sys.argv[1]).resolve()
destination = Path(sys.argv[2]).resolve()
base = json.loads(Path(sys.argv[3]).read_text())['files'] if len(sys.argv) > 3 else {}
payload = app / 'Contents/Resources/Payload/PhoneticIPA.app'
resources = app / 'Contents/Resources'
helper = resources / 'Registration/RegisterInputSource.app'
script = resources / 'Scripts/install-macos.sh'
assert app.is_dir() and app.name == '安装国际音标输入法.app'
subprocess.run(['/usr/bin/codesign', '--verify', '--deep', '--strict', str(app)], check=True)
for path in [app / 'Contents/MacOS/Installer', helper / 'Contents/MacOS/RegisterInputSource', payload / 'Contents/MacOS/PhoneticIPA-Mac']:
    assert set(subprocess.check_output(['/usr/bin/lipo', '-archs', str(path)], text=True).split()) == {'arm64', 'x86_64'}
    commands = subprocess.check_output(['/usr/bin/otool', '-l', str(path)], text=True)
    assert '/Applications/Xcode' not in commands and '/Library/Developer' not in commands
    builds = subprocess.check_output(['/usr/bin/vtool', '-show-build', str(path)], text=True)
    assert 'minos 13.0' in builds and 'MACOS' in builds

qa_roots = sorted(root.glob('build/installer-qa-*/result.json'))
assert qa_roots
qa = qa_roots[-1].parent
assert json.loads((qa / 'result.json').read_text())['passed']
target = Path.home() / 'Library/Input Methods/PhoneticIPA.app'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = {p.relative_to(target).as_posix(): sha(p) for p in target.rglob('*') if p.is_file()}
corrupt = qa / 'corrupt-helper.app'
shutil.copytree(helper, corrupt)
with (corrupt / 'Contents/MacOS/RegisterInputSource').open('ab') as file:
    file.write(b'corrupt-signature-test')
environment = dict(os.environ, IPA_MAC_APP=str(payload), IPA_REGISTRATION_APP=str(corrupt), DEVELOPER_DIR='/__ipa_qa_no_developer_tools__', PATH='/usr/bin:/bin:/usr/sbin:/sbin')
with (qa / 'corrupt-helper.log').open('wb') as log:
    result = subprocess.run(['/bin/bash', str(script), '--update', '--no-open'], env=environment, stdout=log, stderr=subprocess.STDOUT)
assert result.returncode != 0
assert before == {p.relative_to(target).as_posix(): sha(p) for p in target.rglob('*') if p.is_file()}
home = qa / 'symlink-home'
folder = home / 'Library/Input Methods'; folder.mkdir(parents=True)
(folder / 'PhoneticIPA.app').symlink_to(target, target_is_directory=True)
environment = dict(environment, HOME=str(home), IPA_REGISTRATION_APP=str(helper))
with (qa / 'symlink-target.log').open('wb') as log:
    result = subprocess.run(['/bin/bash', str(script), '--update', '--no-open'], env=environment, stdout=log, stderr=subprocess.STDOUT)
assert result.returncode != 0
assert before == {p.relative_to(target).as_posix(): sha(p) for p in target.rglob('*') if p.is_file()}
record = {'corrupt_helper_rejected_before_mutation': True, 'symlink_target_rejected_before_mutation': True, 'architectures_and_macos13_checked': True, 'codesign_strict': True}
wrong = qa / 'wrong-product.bundle/Contents'
wrong.mkdir(parents=True)
(wrong / 'Info.plist').write_bytes(plistlib.dumps({'CFBundleIdentifier': 'org.example.other-product'}))
result = subprocess.run([str(helper / 'Contents/MacOS/RegisterInputSource'), '--exchange-contents', str(target / 'Contents'), str(wrong)], capture_output=True)
assert result.returncode == 2
assert before == {p.relative_to(target).as_posix(): sha(p) for p in target.rglob('*') if p.is_file()}
record['wrong_product_atomic_exchange_rejected_before_mutation'] = True
(qa / 'negative-and-binary-checks.json').write_text(json.dumps(record, indent=2))
manifest = {}
for path in app.rglob('*'):
    assert not path.is_symlink(), path
    if path.is_file():
        manifest[path.relative_to(app).as_posix()] = {'bytes': path.stat().st_size, 'sha256': sha(path), 'mode': path.stat().st_mode & 0o777}
with zipfile.ZipFile(destination, 'x', zipfile.ZIP_DEFLATED) as archive:
    archive.writestr('installer-manifest.json', json.dumps(manifest, indent=2))
    for path in app.rglob('*'):
        if path.is_file():
            if payload in path.parents:
                previous = base.get(path.relative_to(payload).as_posix())
                if previous and previous['sha256'] == sha(path):
                    continue
            archive.write(path, 'installer/' + path.relative_to(app).as_posix())
    for path in qa.iterdir():
        if path.is_file() and path.name != 'previous-session.json':
            archive.write(path, 'evidence/' + path.name)
    for name in ['local-016-build.log', 'local-016-snapshots.log', 'installer-016-build.log', 'installer-016-host.log', 'installer-016-qa.log']:
        if (root / 'build' / name).is_file():
            archive.write(root / 'build' / name, 'evidence/' + name)
print(json.dumps({'bytes': destination.stat().st_size, 'sha256': sha(destination), 'files': len(manifest), 'qa': str(qa)}))
