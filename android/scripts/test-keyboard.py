"""Run real IME tests only on the recorded, project-owned headless emulator."""
import argparse
import json
from pathlib import Path
import subprocess
import time

p = argparse.ArgumentParser()
p.add_argument('--adb', type=Path, required=True)
p.add_argument('--output', type=Path, required=True)
a = p.parse_args()
record = json.loads((a.output/'owned-device.json').read_text(encoding='utf-8'))
assert record['serial'] == 'emulator-5564' and 'Phonetic_IPA_QA' in record['avd']
base = [str(a.adb), '-s', record['serial']]
name = subprocess.check_output(base+['emu', 'avd', 'name'], timeout=15, text=True).splitlines()[0].strip()
assert name == 'Phonetic_IPA_QA_20261023', 'The serial no longer identifies this project emulator'
run = a.output / time.strftime('verified-%Y%m%d-%H%M%S')
run.mkdir(parents=True, exist_ok=False)
runner = 'org.nephelium.phoneticipa.test/org.nephelium.phoneticipa.CoreTests'
for suite in ['core', 'keyboard']:
    command = base+['shell', 'am', 'instrument', '-w', '-r']
    if suite == 'keyboard': command += ['-e', 'suite', 'keyboard']
    result = subprocess.run(command+[runner], capture_output=True, timeout=240)
    log = result.stdout + result.stderr
    (run/(suite+'.log')).write_bytes(log)
    text = log.decode('utf-8', errors='replace')
    print(text, flush=True)
    result.check_returncode()
    assert 'INSTRUMENTATION_CODE: -1' in text and 'passed' in text.lower() and 'FAILURES' not in text and 'AssertionError' not in text, 'Instrumentation did not pass'
subprocess.run(base+['pull', '/sdcard/Android/data/org.nephelium.phoneticipa/files/qa', str(run/'previews')], check=True, timeout=60)
(run/'result.json').write_text(json.dumps({'passed':True, 'serial':record['serial'], 'text':'厌恶wu˥˩a˨˩˦6˥/', 'suites':['JNI/core','real InputMethodService UI']}, ensure_ascii=False, indent=2), encoding='utf-8')
print(run)
