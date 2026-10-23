"""Start a new project-owned headless AVD, without reusing personal emulator data."""
import argparse
import json
import os
from pathlib import Path
import socket
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--sdk', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
args = p.parse_args()
root = args.output.resolve()
root.mkdir(parents=True, exist_ok=True)
name = 'Phonetic_IPA_QA_20261023'
home = root/'avd'
device = home/(name+'.avd')
if device.exists():
    raise SystemExit('QA AVD already exists; inspect its recorded process before reusing it')
with socket.socket() as probe:
    if probe.connect_ex(('127.0.0.1', 5564)) == 0:
        raise SystemExit('Port 5564 belongs to another emulator')
image = args.sdk/'system-images/android-36.1/google_apis_playstore/x86_64'
assert (image/'system.img').exists()
device.mkdir(parents=True)
(home/(name+'.ini')).write_text('avd.ini.encoding=UTF-8\npath='+str(device)+'\ntarget=android-36.1\n', encoding='utf-8')
config = {
    'AvdId': name, 'avd.ini.displayname': 'Phonetic IPA QA', 'avd.ini.encoding': 'UTF-8',
    'abi.type': 'x86_64', 'hw.cpu.arch': 'x86_64', 'hw.cpu.ncore': '2',
    'hw.ramSize': '2048', 'hw.keyboard': 'yes', 'hw.gpu.enabled': 'yes', 'hw.gpu.mode': 'swiftshader',
    'hw.lcd.width': '1080', 'hw.lcd.height': '2400', 'hw.lcd.density': '420',
    'hw.mainKeys': 'no', 'disk.dataPartition.size': '4G', 'image.sysdir.1': str(image)+os.sep,
    'tag.id': 'google_apis_playstore', 'tag.display': 'Google Play', 'target': 'android-36.1',
    'hw.audioInput': 'no', 'hw.camera.back': 'none', 'hw.camera.front': 'none', 'showDeviceFrame': 'no',
    'fastboot.forceColdBoot': 'yes', 'fastboot.forceFastBoot': 'no'
}
(device/'config.ini').write_text(''.join(k+'='+v+'\n' for k,v in config.items()), encoding='utf-8')
environment = dict(os.environ, ANDROID_AVD_HOME=str(home), ANDROID_HOME=str(args.sdk))
command = [str(args.sdk/'emulator/emulator.exe'), '-avd', name, '-port', '5564', '-no-window', '-no-audio', '-no-snapshot', '-no-boot-anim', '-gpu', 'swiftshader']
with (root/'emulator.log').open('wb') as log:
    process = subprocess.Popen(command, env=environment, stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW)
(root/'owned-device.json').write_text(json.dumps({'pid':process.pid,'serial':'emulator-5564','avd':str(device),'command':command},indent=2), encoding='utf-8')
print(json.dumps({'pid':process.pid,'serial':'emulator-5564','avd':str(device)}))
