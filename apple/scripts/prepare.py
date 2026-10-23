"""Build reproducible Apple lookup data from fixed resources and factory vocabulary."""
import argparse
import hashlib
import json
import re
import shutil
import sqlite3
from pathlib import Path


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--resources', type=Path, required=True)
    p.add_argument('--factory-dictionary', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--index-only', action='store_true', help='Generate only the index beside already staged resources')
    args = p.parse_args()
    expected = '9e68e88be036206fc14d44db7111376b1d92f9173b04d623e601e31edd664c2f'
    if sha(args.factory_dictionary) != expected:
        raise ValueError('Expected fixed factory msime.db; user dictionaries are not accepted')
    if args.index_only:
        if not args.output.is_dir() or (args.output/'lookup.sqlite').exists():
            raise ValueError('Index-only mode needs a staged resource folder without an existing index')
        if sha(args.resources/'word-readings.tsv') != '7b1b584761ac69c48f5b00fe6a87ddad76b24df7cdd4fc20e5e18741740e7cad':
            raise ValueError('Frozen word readings differ')
    else:
        manifest = json.loads((args.resources.parent/'manifest.json').read_text(encoding='utf-8-sig'))
        for name, digest in manifest.items():
            if sha(args.resources/name) != digest:
                raise ValueError('Frozen resource differs: '+name)
        args.output.mkdir(parents=True, exist_ok=False)
        for name in manifest:
            if name != 'word-readings.tsv':
                shutil.copy2(args.resources/name, args.output/name)
        prototype = args.resources.parent.parent
        shutil.copy2(prototype/'THIRD_PARTY.md', args.output/'THIRD_PARTY.md')
        shutil.copy2(args.resources.parent/'provenance.json', args.output/'provenance.json')
        shutil.copy2(prototype.parent/'THIRD_PARTY_NOTICES.txt', args.output/'UPSTREAM_NOTICES.txt')
    source = sqlite3.connect(args.factory_dictionary.resolve().as_uri()+'?mode=ro&immutable=1', uri=True)
    target = sqlite3.connect(args.output/'lookup.sqlite')
    target.executescript('''
      PRAGMA page_size=4096;
      CREATE TABLE candidates(code TEXT,word TEXT,canonical TEXT,weight INTEGER);
      CREATE TABLE readings(word TEXT,reading TEXT);
    ''')
    count = 0
    for (table,) in source.execute("SELECT name FROM sqlite_master WHERE type='table' ORDER BY name"):
        if not re.fullmatch(r'tbl_(?:[1-7]|others)_[a-z]', table):
            continue
        rows = source.execute(f'SELECT "key",value,weight FROM "{table}"')
        batch = []
        for spelling, word, weight in rows:
            if not spelling or not word:
                continue
            canonical = spelling.lower().replace('ü','v').replace('u:','v')
            if not re.fullmatch(r"[a-z]+(?:'[a-z]+)*", canonical):
                continue
            batch.append((canonical.replace("'",''),word,canonical,weight or 0))
        target.executemany('INSERT INTO candidates VALUES(?,?,?,?)',batch)
        count += len(batch)
    readings=[]
    for line in (args.resources/'word-readings.tsv').read_text(encoding='utf-8').splitlines():
        word, reading=line.split('\t')
        readings.append((word,reading))
    target.executemany('INSERT INTO readings VALUES(?,?)',readings)
    target.executescript('CREATE INDEX candidate_lookup ON candidates(code,weight DESC); CREATE INDEX word_reading ON readings(word);')
    target.commit()
    assert target.execute("SELECT count(*) FROM candidates WHERE code='wu' AND word='恶'").fetchone()[0] > 0
    assert target.execute("SELECT count(*) FROM readings WHERE word='好学'").fetchone()[0] == 2
    target.close()
    source.close()
    record={'factory_sha256':expected,'candidate_rows':count,'reading_rows':len(readings),'sqlite_version':sqlite3.sqlite_version,
            'source':'metasequoiaime/MSIME-Engine dictionary at d0dc0c2b594b5540b5de99ad12085c786410626e',
            'files':{f.name:sha(f) for f in sorted(args.output.iterdir()) if f.is_file() and f.name!='apple-resource-manifest.json'}}
    (args.output/'apple-resource-manifest.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'candidate_rows':count,'reading_rows':len(readings),'database_bytes':(args.output/'lookup.sqlite').stat().st_size}))


if __name__ == '__main__':
    main()
