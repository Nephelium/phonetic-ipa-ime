"""Prepare the prototype's isolated resources; never modifies the source toolbox/CIN."""
from pathlib import Path
import hashlib
import json
import shutil
import urllib.request
from prepare_default_keymap import prepare_default_keymap
from prepare_readings import main as prepare_readings
from prepare_plus_catalog import prepare_catalog

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / 'msime-windows'
DATA = ROOT / 'output' / 'runtime' / 'data'
TOOLBOX = Path(r'D:\PhoneticToolbox\PhoneticToolbox_v3')
CIN = Path(r'C:\Users\13680\Documents\ipa.cin')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    DATA.mkdir(parents=True, exist_ok=True)
    ipa = DATA / 'ipa'
    ipa.mkdir(exist_ok=True)
    source = TOOLBOX / 'frontend/src/modules/mandarin-ipa/ipa-data.json'
    payload = json.loads(source.read_text(encoding='utf-8-sig'))
    rows = payload['rows']
    with (ipa / 'beijing.tsv').open('w', encoding='utf-8', newline='\n') as out:
        for row in rows:
            if row[3]:
                out.write('\t'.join(str(row[i]) for i in (0, 2, 3)) + '\n')
    shutil.copy2(CIN, ipa / 'ipa.cin')
    prepare_default_keymap(ipa)
    prepare_readings()
    for name in ('DoulosSIL-Regular.ttf', 'Doulos-OFL.txt'):
        shutil.copy2(TOOLBOX / 'frontend/src/assets' / name, ipa / name)
    plus = TOOLBOX / 'frontend/src/modules/ipa-plus'
    catalog_source = plus / 'data/catalog.json'
    prepare_catalog(catalog_source, ipa / 'plus-catalog.json')
    shutil.copy2(plus / 'data/sources.json', ipa / 'plus-sources.json')
    shutil.copy2(plus / 'README.md', ipa / 'Plus-README.md')
    shutil.copytree(TOOLBOX / 'frontend/src/assets/ipa-plus', ipa / 'plus-font-license', dirs_exist_ok=True)
    shutil.copy2(TOOLBOX / 'frontend/src/assets/ipa-plus/PTBIPAPlus-Regular.ttf', ipa / 'PTBIPAPlus-Regular.ttf')
    provenance = {'rule': 'Beijing', 'source': str(source), 'source_sha256': sha(source),
                  'cin_source': str(CIN), 'cin_sha256': sha(CIN), 'readings': len(rows),
                  'plus': {'source': str(catalog_source), 'source_sha256': sha(catalog_source),
                           'corrections': [{'entry': 'ipa-combinations-021',
                                            'reason': 'Attach the voiceless ring to n before palatalization',
                                            'from': 'n\u0320\u02b2\u030a', 'to': 'n\u0320\u030a\u02b2'}]},
                  'artifacts': {p.name: sha(p) for p in ipa.iterdir() if p.is_file() and p.name != 'provenance.json'}}
    (ipa / 'provenance.json').write_text(json.dumps(provenance, ensure_ascii=False, indent=2), encoding='utf-8')
    lock = json.loads((UPSTREAM / 'product-lock.json').read_text())['dictionary']
    cache = ROOT / 'output' / 'dictionary-downloads' / lock['tag']
    cache.mkdir(parents=True, exist_ok=True)
    for name, expected in lock['assets'].items():
        dest = DATA / name
        # The engine writes learning information into its runtime databases.
        # Verify a separate immutable download, and never replace an existing DB.
        pinned = cache / name
        if not pinned.exists() and dest.exists() and sha(dest) == expected:
            shutil.copy2(dest, pinned)
        if not pinned.exists() or sha(pinned) != expected:
            url = f"https://github.com/{lock['repository']}/releases/download/{lock['tag']}/{name}"
            if not url.startswith('https://github.com/'):
                raise RuntimeError(f'Unexpected asset URL: {url}')
            print('Downloading', name, flush=True)
            req = urllib.request.Request(url, headers={'User-Agent': 'PhoneticIpa-Prep/1.0'})
            with urllib.request.urlopen(req, timeout=180) as response:
                content = response.read()
            if hashlib.sha256(content).hexdigest() != expected:
                raise RuntimeError(f'Hash mismatch: {name}')
            pinned.write_bytes(content)
        if not dest.exists():
            shutil.copy2(pinned, dest)
        elif sha(dest) != expected:
            # A dictionary from an older lock (or one with local learning data) must
            # not be packaged. Keep it beside the new file instead of overwriting it.
            kept = dest.with_name(dest.name + '.before-' + lock['tag'])
            if not kept.exists():
                shutil.copy2(dest, kept)
            shutil.copy2(pinned, dest)
            print('Replaced', name, 'with locked dictionary; previous copy kept as', kept.name, flush=True)
    shutil.copytree(UPSTREAM / 'engine/helpcode/helpcodes', DATA / 'helpcodes', dirs_exist_ok=True)
    shutil.copytree(UPSTREAM / 'server/assets/tables', DATA, dirs_exist_ok=True)
    (DATA / 'radical').mkdir(exist_ok=True)
    shutil.copy2(UPSTREAM / 'engine/radical/decomposition.txt', DATA / 'radical/decomposition.txt')
    print('Prepared', DATA, flush=True)

if __name__ == '__main__':
    main()
