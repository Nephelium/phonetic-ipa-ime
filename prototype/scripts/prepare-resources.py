"""Prepare isolated runtime resources from the attributed, hash-locked snapshot."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--skip-dictionaries', action='store_true', help='Only prepare the IPA resources')
    args = parser.parse_args()
    data = ROOT / 'output/runtime/data'
    ipa = data / 'ipa'
    ipa.mkdir(parents=True, exist_ok=True)
    manifest = json.loads((ROOT / 'resources/manifest.json').read_text(encoding='utf-8'))
    for name, expected in manifest.items():
        source = ROOT / 'resources/ipa' / name
        if digest(source) != expected:
            raise ValueError(f'Resource hash mismatch: {name}')
        shutil.copy2(source, ipa / name)
    # The activity/learning databases and active.cin are never overwritten here.
    shutil.copy2(ROOT / 'THIRD_PARTY.md', ipa / 'THIRD_PARTY.md')
    shutil.copy2(ROOT / 'resources/provenance.json', ipa / 'provenance.json')
    for name in ('default', 'ipa-paper', 'ipa-celadon', 'ipa-night'):
        shutil.copytree(REPO / 'skins' / name, data / 'skins' / name, dirs_exist_ok=True)
    if not args.skip_dictionaries:
        lock = json.loads((REPO / 'product-lock.json').read_text(encoding='utf-8'))['dictionary']
        cache = ROOT / 'output/dictionary-downloads' / lock['tag']
        cache.mkdir(parents=True, exist_ok=True)
        for name, expected in lock['assets'].items():
            frozen = cache / name
            if not frozen.exists() or digest(frozen) != expected:
                url = f"https://github.com/{lock['repository']}/releases/download/{lock['tag']}/{name}"
                with urllib.request.urlopen(url, timeout=180) as response:
                    body = response.read()
                if hashlib.sha256(body).hexdigest() != expected:
                    raise ValueError(f'Dictionary hash mismatch: {name}')
                frozen.write_bytes(body)
            if not (data / name).exists():
                shutil.copy2(frozen, data / name)
    shutil.copytree(REPO / 'engine/helpcode/helpcodes', data / 'helpcodes', dirs_exist_ok=True)
    shutil.copytree(REPO / 'server/assets/tables', data, dirs_exist_ok=True)
    (data / 'radical').mkdir(exist_ok=True)
    shutil.copy2(REPO / 'engine/radical/decomposition.txt', data / 'radical/decomposition.txt')
    print(f'Prepared {data}')


if __name__ == '__main__':
    main()
