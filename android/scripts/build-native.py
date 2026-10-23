"""Compile the existing IPA core for Android; no global SDK changes."""
import argparse
import os
from pathlib import Path
import subprocess

p = argparse.ArgumentParser()
p.add_argument('--ndk', type=Path, required=True)
p.add_argument('--source-root', type=Path)
args = p.parse_args()
root = Path(__file__).resolve().parents[1]
source = args.source_root or (root.parent/'msime-windows' if (root.parent/'msime-windows').is_dir() else root.parent)
assert (source/'engine/ipa/ipa_readings.h').is_file(), 'IPA_SOURCE_ROOT must contain engine/ipa'
prebuilt = args.ndk/'toolchains/llvm/prebuilt'/('windows-x86_64' if os.name == 'nt' else 'darwin-x86_64')
compiler = prebuilt/'bin'/('clang++.exe' if os.name == 'nt' else 'clang++')
for abi, target in [('arm64-v8a', 'aarch64-linux-android29'), ('x86_64', 'x86_64-linux-android29')]:
    out = root/'app/src/main/jniLibs'/abi
    out.mkdir(parents=True, exist_ok=True)
    subprocess.run([str(compiler), f'--target={target}', '-std=c++17', '-O2', '-shared', '-fPIC', '-static-libstdc++',
                    '-Wl,-z,max-page-size=16384', '-Wl,--build-id=sha1', '-I'+str(source),
                    str(root/'app/src/main/cpp/ipa_jni.cpp'), '-o', str(out/'libphoneticipa.so')], check=True)
    print(abi, (out/'libphoneticipa.so').stat().st_size)
