"""Check the distributable itself, including tutorial assets and launchers."""
import argparse
import hashlib
import json
from pathlib import Path
import tomllib
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('package', type=Path)
args = parser.parse_args()
with zipfile.ZipFile(args.package) as archive:
    assert archive.testzip() is None, 'Corrupt ZIP member'
    hashes = json.loads(archive.read('SHA256.json'))
    assert set(archive.namelist()) == set(hashes) | {'SHA256.json'}, 'Incomplete package hash list'
    for name, expected in hashes.items():
        assert hashlib.sha256(archive.read(name)).hexdigest() == expected, name
    binaries = json.loads(archive.read('output/runtime/build-manifest.json'))
    for name, expected in binaries.items():
        assert hashlib.sha256(archive.read('output/runtime/' + name.replace('\\', '/'))).hexdigest() == expected, name
    for skin in ['bigfish', 'bigfish-peek', 'bigfish-national-day']:
        prefix = 'output/runtime/data/skins/' + skin + '/'
        manifest = tomllib.loads(archive.read(prefix + 'skin.toml').decode('utf-8-sig'))
        assert manifest['id'] == skin
        for layer in ['background', 'decoration']:
            asset = manifest['candidate_window'][layer]['image']
            assert archive.read(prefix + asset), skin + '/' + asset
    config = tomllib.loads(archive.read('output/runtime/data/config.toml').decode('utf-8-sig'))
    assert config['appearance']['candidate_skin'] == 'bigfish', 'Ordinary blue bigfish must be the factory default'
    assert archive.read('output/runtime/data/assets/settings/ipa-bigfish.png')
    assert b'-Guide' in archive.read('使用教学.cmd')
    assert b'-Guide' in archive.read('scripts/Install-Windows.ps1')
    assert b'--guide' in archive.read('scripts/Start-Settings.ps1')
    for script in ['scripts/Install-Windows.ps1', 'scripts/Start-Settings.ps1']:
        assert archive.read(script).startswith(b'\xef\xbb\xbf'), script + ' must support PowerShell 5.1'
    assert not any((Path(name).name.startswith('onboarding-') and name.endswith('.done')) for name in archive.namelist()), 'User completion marker in package'
    assert not any(Path(name).name.startswith('custom-icon-') for name in archive.namelist()), 'Personal picture in package'
print('Windows package verified: member hashes, three bigfish skins, tutorial launchers, clean completion state')
