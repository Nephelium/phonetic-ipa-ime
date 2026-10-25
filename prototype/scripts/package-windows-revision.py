"""Revise a verified local Windows package without importing personal data."""
import argparse
import copy
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('base', type=Path)
parser.add_argument('--settings', type=Path, required=True)
parser.add_argument('--scripts', type=Path, required=True)
parser.add_argument('--skins', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
parser.add_argument('--readme', type=Path)
parser.add_argument('--runtime', type=Path)
parser.add_argument('--dll-x64', type=Path)
parser.add_argument('--dll-x86', type=Path)
parser.add_argument('--icon', type=Path)
parser.add_argument('--guide-assets', type=Path)
parser.add_argument('--third-party', type=Path)
args = parser.parse_args()
sha = lambda data: hashlib.sha256(data).hexdigest()
replacements = {'output/runtime/PhoneticIpaSettings.exe': args.settings.read_bytes()}
if args.runtime:
    for name in ['PhoneticIpaServer.exe', 'PhoneticIpaSettings.exe', 'PhoneticIpaWatchdog.exe']:
        replacements['output/runtime/' + name] = (args.runtime / name).read_bytes()
for arch, path in [('x64', args.dll_x64), ('x86', args.dll_x86)]:
    if path:
        replacements['output/runtime/tsf-' + arch + '/PhoneticIpaTsf.dll'] = path.read_bytes()
if args.icon:
    replacements['output/runtime/data/assets/settings/ipa-bigfish.png'] = args.icon.read_bytes()
if args.guide_assets:
    for path in args.guide_assets.rglob('*'):
        if path.is_file():
            replacements[path.relative_to(args.guide_assets).as_posix()] = path.read_bytes()
if args.third_party:
    replacements['THIRD_PARTY.md'] = args.third_party.read_bytes()
for name in ['Install-Windows.ps1', 'Start-Settings.ps1', 'Register-Prototype.ps1', 'Windows-Installation.ps1', 'Uninstall-Windows.ps1']:
    replacements['scripts/' + name] = (args.scripts / name).read_bytes()
for skin in ['bigfish', 'bigfish-peek', 'bigfish-national-day']:
    for path in (args.skins / skin).rglob('*'):
        if path.is_file():
            replacements['output/runtime/data/skins/' + skin + '/' + path.relative_to(args.skins / skin).as_posix()] = path.read_bytes()
replacements['使用教学.cmd'] = b'@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\\Start-Settings.ps1" -Guide\r\nif errorlevel 1 pause\r\n'
replacements['卸载.cmd'] = b'@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\\Uninstall-Windows.ps1"\r\nset "result=%errorlevel%"\r\npause\r\nexit /b %result%\r\n'
if args.readme:
    replacements['安装说明.md'] = args.readme.read_bytes()
with zipfile.ZipFile(args.base) as source:
    assert source.testzip() is None
    original = json.loads(source.read('SHA256.json'))
    for name, digest in original.items():
        assert sha(source.read(name)) == digest, 'Base package changed: ' + name
    binaries = json.loads(source.read('output/runtime/build-manifest.json'))
    for name, data in replacements.items():
        if name.startswith('output/runtime/') and name.endswith(('.exe', '.dll')):
            binaries[name.removeprefix('output/runtime/').replace('/', '\\')] = sha(data)
    replacements['output/runtime/build-manifest.json'] = json.dumps(binaries, indent=2).encode()
    config = source.read('output/runtime/data/config.toml').decode('utf-8-sig')
    replacements['output/runtime/data/config.toml'] = config.replace('candidate_skin = "fluent"', 'candidate_skin = "bigfish"').encode()
    assert sha(source.read('output/runtime/data/msime.db')) == '9e68e88be036206fc14d44db7111376b1d92f9173b04d623e601e31edd664c2f'
    hashes = {}
    with zipfile.ZipFile(args.output, 'x', zipfile.ZIP_DEFLATED, compresslevel=6) as target:
        for info in source.infolist():
            name = info.filename
            if name == 'SHA256.json':
                continue
            data = replacements.pop(name, None)
            if data is None:
                data = source.read(name)
            hashes[name] = sha(data)
            target.writestr(copy.copy(info), data)
        for name, data in replacements.items():
            hashes[name] = sha(data)
            target.writestr(name, data)
        target.writestr('SHA256.json', json.dumps(hashes, ensure_ascii=False, indent=2).encode())
subprocess.run([sys.executable, str(Path(__file__).with_name('verify-windows-package.py')), str(args.output)], check=True)
print(json.dumps({'package': str(args.output), 'bytes': args.output.stat().st_size, 'sha256': sha(args.output.read_bytes())}, ensure_ascii=False, indent=2))
