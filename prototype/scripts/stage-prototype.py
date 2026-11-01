"""Stage built prototype files and notices, without registration or process launches."""
from pathlib import Path
import hashlib
import json
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent
OUT = ROOT / 'output'
RUNTIME = OUT / 'runtime'
RUNTIME.mkdir(parents=True, exist_ok=True)
# CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE may be redirected in the build cache; staging must
# follow it, or an old bin/Release copy would be packaged instead of the fresh build.
server = OUT / 'build-server/bin/Release'
cache = OUT / 'build-server/CMakeCache.txt'
if cache.exists():
    for line in cache.read_text(encoding='utf-8', errors='replace').splitlines():
        if line.startswith('CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE:'):
            server = Path(line.split('=', 1)[1].strip())
for name in ('PhoneticIpaServer.exe', 'PhoneticIpaSettings.exe', 'PhoneticIpaWatchdog.exe'):
    shutil.copy2(server / name, RUNTIME / name)
for binary in server.glob('*.dll'):
    shutil.copy2(binary, RUNTIME / binary.name)
for arch in ('x64', 'x86'):
    dest = RUNTIME / f'tsf-{arch}'
    dest.mkdir(exist_ok=True)
    shutil.copy2(OUT / f'build-{arch}/Release/PhoneticIpaTsf.dll', dest / 'PhoneticIpaTsf.dll')
for skin in (REPO / 'skins').iterdir():
    if not skin.is_dir():
        continue
    manifest = skin / 'skin.toml'
    if manifest.exists() and re.search(r'(?m)^\s*assets\s*=\s*"UNVERIFIED', manifest.read_text(encoding='utf-8')):
        continue  # Same distribution exclusion as upstream Prepare-PackageFiles.ps1.
    shutil.copytree(skin, RUNTIME / 'data/skins' / skin.name, dirs_exist_ok=True)
shutil.copytree(REPO / 'server/assets/settings', RUNTIME / 'data/assets/settings', dirs_exist_ok=True)
for name in ('LICENSE', 'THIRD_PARTY_NOTICES.txt', 'MODIFICATIONS.md'):
    shutil.copy2(REPO / name, RUNTIME / name)
shutil.copy2(ROOT / 'THIRD_PARTY.md', RUNTIME / 'IPA-SOURCES.md')
manifest = {str(p.relative_to(RUNTIME)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in RUNTIME.rglob('*') if p.is_file() and p.suffix in ('.exe', '.dll')}
(RUNTIME / 'build-manifest.json').write_text(json.dumps(manifest, indent=2), encoding='utf-8')
print(f'Staged {RUNTIME}')
