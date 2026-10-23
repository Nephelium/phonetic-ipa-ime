"""Resample the original generated artwork into platform-required icon sizes (macOS sips)."""
import json
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
source = root / 'assets/icons/ipa-bird-concept-v1.png'
catalog = root / 'apple/Assets.xcassets'
folder = catalog / 'AppIcon.appiconset'
folder.mkdir(parents=True, exist_ok=True)
images = []
for size in (16, 32, 128, 256, 512):
    for scale in (1, 2):
        filename = f'icon-{size * scale}.png'
        images.append({'idiom': 'mac', 'size': f'{size}x{size}', 'scale': f'{scale}x', 'filename': filename})
images.append({'idiom': 'universal', 'platform': 'ios', 'size': '1024x1024', 'filename': 'icon-1024.png'})
for pixels in sorted({16, 32, 64, 128, 256, 512, 1024}):
    subprocess.run(['sips', '-z', str(pixels), str(pixels), str(source), '--out', str(folder / f'icon-{pixels}.png')], check=True, stdout=subprocess.DEVNULL)
info = {'author': 'xcode', 'version': 1}
(catalog / 'Contents.json').write_text(json.dumps({'info': info}, indent=2) + '\n')
(folder / 'Contents.json').write_text(json.dumps({'images': images, 'info': info}, indent=2) + '\n')
print(folder)
