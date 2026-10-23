"""Build editable prototype defaults without changing the original CIN or active profile."""
from pathlib import Path


def remap_reserved_one(text: str) -> str:
    """Move bare 1 candidates to backslash; preserve literal backslash and raw outputs."""
    prefix, rest = text.split('%chardef begin', 1)
    body, suffix = rest.split('%chardef end', 1)
    lines = body.splitlines()
    moved, existing, kept = [], [], []
    for line in lines:
        parts = line.split(maxsplit=1)
        if len(parts) == 2 and parts[0] == '1':
            moved.append(parts[1])
        elif len(parts) == 2 and parts[0] == '\\':
            existing.append(parts[1])
        else:
            kept.append(line)
    if not moved:
        return text
    rebuilt = '\n'.join(kept).rstrip('\n') + '\n'
    rebuilt += ''.join(f'\\\t{value}\n' for value in moved + existing)
    return prefix + '%chardef begin' + rebuilt + '%chardef end' + suffix


def prepare_default_keymap(directory: Path) -> None:
    original = (directory / 'ipa.cin').read_text(encoding='utf-8-sig')
    body = original.split('%chardef begin', 1)[1].split('%chardef end', 1)[0]
    existing = {line.split()[0] for line in body.splitlines() if line.strip()}
    additions = ''.join(f'{key}\t{output}\n' for key, output in zip('!@#$%', '¹²³⁴⁵')
                        if key not in existing)
    defaults = remap_reserved_one(original.replace('%chardef end', additions + '%chardef end'))
    (directory / 'default.cin').write_text(defaults,
                                          encoding='utf-8', newline='\n')


if __name__ == '__main__':
    prepare_default_keymap(Path(__file__).resolve().parents[1] / 'output/runtime/data/ipa')
