"""Check the nasal's mark attachment in displayed and inserted IPA Plus text."""
import argparse
import json
from pathlib import Path
import unicodedata


def check(path):
    catalog = json.loads(path.read_text(encoding='utf-8-sig'))
    entries = catalog['entries']
    by_id = {entry['id']: entry for entry in entries}
    assert len(entries) == len(by_id) == 625, 'Catalog identities/count changed'
    entry = by_id['ipa-combinations-021']
    expected = 'n\u0320\u030a\u02b2'
    assert entry['display'] == expected, 'Display attaches the ring to the wrong letter'
    assert entry['insertText'] == expected, 'Insertion attaches the ring to the wrong letter'
    assert entry['codePoints'] == [f'U+{ord(char):04X}' for char in expected], 'Stale code points'
    assert entry['nameEn'] == 'combined transcription: ' + expected, 'Stale English example'
    for symbol in entries:
        if symbol['system'] == 'ipa' and symbol['section'] == 'combinations':
            for text in (symbol['display'], symbol['insertText']):
                assert not any(unicodedata.category(a) == 'Lm' and b in '\u030a\u0325'
                               for a, b in zip(text, text[1:])), symbol['id']
    for identifier, text in (('018', 'n\u0320\u030a'), ('019', 'n\u0320'), ('022', 'n\u0320\u02b2')):
        neighbor = by_id['ipa-combinations-' + identifier]
        assert neighbor['display'] == neighbor['insertText'] == text, 'Neighboring nasal changed'
    print(f'PASS {path}: display, insertion, code points, neighbors and 625 identities')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('catalogs', type=Path, nargs='*',
                        default=[Path(__file__).resolve().parents[1] / 'resources/ipa/plus-catalog.json'])
    for path in parser.parse_args().catalogs:
        check(path)
