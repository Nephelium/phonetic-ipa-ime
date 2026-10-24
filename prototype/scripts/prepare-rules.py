"""Freeze every transcription column from the same PhoneticToolbox source, read-only."""
from pathlib import Path
import json, hashlib, argparse

ROOT = Path(__file__).resolve().parents[1]
RULES = [
    ('beijing', 'Standard Chinese Beijing 宽'), ('beijing-narrow', 'Standard Chinese Beijing 严'), ('hu-yushu', '胡裕树'),
    ('huang-liao', '黄廖'), ('qian-nairong', '钱乃荣'), ('wu-zongji', '吴宗济'),
    ('chao-yuen-ren', '赵元任'), ('fangyin-zihui', '方音字汇'),
    ('unt-broad', 'UntPhesoca 宽'), ('unt-narrow', 'UntPhesoca 严'),
]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, default=Path('D:/PhoneticToolbox/PhoneticToolbox_v3/frontend/src/modules/mandarin-ipa/ipa-data.json'))
    parser.add_argument('--output', type=Path, default=ROOT/'output/runtime/data/ipa')
    args = parser.parse_args()
    raw = args.source.read_bytes()
    payload = json.loads(raw.decode('utf-8-sig'))
    assert len(payload['columns']) == len(RULES) + 3
    args.output.mkdir(parents=True, exist_ok=True)
    catalog = []
    record = {'source': 'PhoneticToolbox v3 frontend/src/modules/mandarin-ipa/ipa-data.json',
              'sha256': hashlib.sha256(raw).hexdigest(), 'schema': payload['schema_version'],
              'source_sha256': payload['source_sha256'], 'unt_revision': payload['unt_revision'],
              'changes': 'Extracted each transcription column with Hanzi, pinyin and tone; original IPA strings retained.',
              'rules': []}
    for column, (rule_id, short) in enumerate(RULES, 3):
        name = payload['columns'][column]
        filename = f'hanzi-rule-{rule_id}.tsv'
        text = ''.join(f'{r[0]}\t{r[2]}\t{r[column]}\t{r[1]}\n' for r in payload['rows'] if r[column])
        data = text.encode('utf-8')
        (args.output/filename).write_bytes(data)
        display = short if rule_id in {'beijing', 'beijing-narrow', 'unt-broad', 'unt-narrow'} else name
        catalog.append(f'{rule_id}\t{display}\t{short}\t{filename}\n')
        record['rules'].append({'id': rule_id, 'name': name, 'column': column, 'file': filename,
                                'rows': len(text.splitlines()), 'sha256': hashlib.sha256(data).hexdigest()})
    (args.output/'hanzi-rules.tsv').write_text(''.join(catalog), encoding='utf-8', newline='\n')
    (args.output/'Rules-SOURCES.json').write_text(json.dumps(record, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    print(json.dumps(record, ensure_ascii=False, indent=2))

if __name__ == '__main__':
    main()
