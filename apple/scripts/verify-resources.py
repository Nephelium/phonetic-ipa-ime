"""Verify fixed build resources; never repair or rewrite a mismatched manifest."""
import argparse
import hashlib
import json
from pathlib import Path
import re

parser = argparse.ArgumentParser()
parser.add_argument('directory', type=Path, nargs='?', default=Path(__file__).resolve().parents[1] / 'Generated/IPA')
parser.add_argument('--manifest', type=Path)
args = parser.parse_args()
manifest = json.loads((args.manifest or args.directory / 'apple-resource-manifest.json').read_text(encoding='utf-8-sig'))
files = manifest['files']
if not isinstance(files, dict) or not {'lookup.sqlite', 'default.cin', 'hanzi-rules.tsv'}.issubset(files):
    raise ValueError('Incomplete fixed-resource manifest')
for name, expected in files.items():
    if not isinstance(name, str) or name != Path(name).name or '/' in name or '\\' in name or name in {'.', '..'}:
        raise ValueError('Invalid resource filename')
    if not isinstance(expected, str) or not re.fullmatch(r'[0-9a-f]{64}', expected):
        raise ValueError('Invalid resource digest: ' + name)
    digest = hashlib.sha256()
    with (args.directory / name).open('rb') as resource:
        for chunk in iter(lambda: resource.read(1024 * 1024), b''):
            digest.update(chunk)
    if digest.hexdigest() != expected:
        raise ValueError('Fixed resource differs from manifest: ' + name)
print('Fixed resource manifest verified: ' + str(len(files)) + ' files')
