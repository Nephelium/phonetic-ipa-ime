"""Only resize/encode the approved mascot artwork for native application resources."""
from pathlib import Path
import json
from PIL import Image

root = next(parent for parent in Path(__file__).resolve().parents if (parent / 'assets/icons/ipa-bigfish-v1.png').is_file())
windows = root / 'msime-windows' if (root / 'msime-windows/server').is_dir() else root
source = root / 'assets/icons/ipa-bigfish-v1.png'
with Image.open(source) as image:
    assert image.width == image.height and image.mode in ['RGB', 'RGBA']
    for name in ['server/src/resource/MetasequoiaIME.ico', 'server/src/resource/Settings.ico', 'windows/image/msime.ico']:
        image.save(windows / name, format='ICO', sizes=[(size, size) for size in [16, 20, 24, 32, 40, 48, 64, 128, 256]])
    image.resize((256, 256), Image.Resampling.LANCZOS).save(windows / 'server/assets/settings/ipa-bigfish.png')
    resources = root / 'android/app/src/main/res'
    drawable = resources / 'drawable-nodpi'
    drawable.mkdir(exist_ok=True)
    image.resize((432, 432), Image.Resampling.LANCZOS).save(drawable / 'ipa_bigfish_art.png')
    for density, size in [('mdpi', 48), ('hdpi', 72), ('xhdpi', 96), ('xxhdpi', 144), ('xxxhdpi', 192)]:
        folder = resources / ('mipmap-' + density)
        folder.mkdir(exist_ok=True)
        image.resize((size, size), Image.Resampling.LANCZOS).save(folder / 'ipa_bigfish.png')
print('Windows and Android application icons prepared from approved artwork')
