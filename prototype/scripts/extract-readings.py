"""Reproduce TSV snapshots from explicitly supplied M13 JSON and CC-CEDICT gzip.

Inputs are read-only. Published input hashes are checked before creating output.
No downloading, registration, active-profile writes, or private path defaults.
"""
from pathlib import Path
import argparse
import gzip
import hashlib
import json
import re


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--m13-json', type=Path, required=True)
    parser.add_argument('--cedict-gzip', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True, help='A new directory')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    provenance = json.loads((root / 'resources/provenance.json').read_text(encoding='utf-8'))
    for path, expected in [(args.m13_json, provenance['ipa_source_sha256']),
                           (args.cedict_gzip, provenance['source_sha256'])]:
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f'Input differs from the published snapshot: {path.name}')
    words = set()
    for line in gzip.decompress(args.cedict_gzip.read_bytes()).decode('utf-8-sig').splitlines():
        match = re.match(r'^(\S+) (\S+) \[([^]]+)\] /', line)
        if not match:
            continue
        traditional, simplified, reading = match.groups()
        syllables = reading.lower().replace('u:', 'v').replace('ü', 'v').split()
        if not all(re.fullmatch(r'[a-zvê]+[1-5]', s) for s in syllables):
            continue
        for word in (traditional, simplified):
            if len(word) < 2 or len(word) != len(syllables) or not all(
                '\u3400' <= c <= '\u9fff' or '\U00020000' <= c <= '\U000323af' for c in word
            ):
                continue
            words.add((word, "'".join(syllables)))
    rows = [r for r in json.loads(args.m13_json.read_text(encoding='utf-8-sig'))['rows'] if r[3]]
    outputs = {
        'word-readings.tsv': ''.join(f'{w}\t{p}\n' for w, p in sorted(words)),
        'beijing.tsv': ''.join(f'{r[0]}\t{r[2]}\t{r[3]}\n' for r in rows),
        'beijing-toned.tsv': ''.join(f'{r[0]}\t{r[2]}\t{r[3]}\t{r[1]}\n' for r in rows),
    }
    # Preserve the exact published snapshot: legacy three-column TSV is LF,
    # while the two later files were exported on Windows as CRLF.
    payloads = {name: (text if name == 'beijing.tsv' else text.replace('\n', '\r\n')).encode('utf-8')
                for name, text in outputs.items()}
    for name, payload in payloads.items():
        if hashlib.sha256(payload).hexdigest() != provenance['artifacts'][name]:
            raise ValueError(f'Regenerated TSV differs: {name}')
    args.output.mkdir(parents=True, exist_ok=False)
    for name, payload in payloads.items():
        (args.output / name).write_bytes(payload)
    print(f'Reproduced {len(outputs)} byte-identical TSV files, {len(words)} word/reading pairs')


if __name__ == '__main__':
    main()
