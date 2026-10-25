#!/bin/bash
# Build-time only: end-user installation never invokes Xcode or a compiler.
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
source_app="${IPA_MAC_APP:-$PWD/build/products/Debug/PhoneticIPA-Mac.app}"
[[ -d "$source_app" ]] || { echo 'Build the input-method app first.' >&2; exit 1; }
codesign --verify --deep --strict "$source_app"
version=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$source_app/Contents/Info.plist")
stamp="${IPA_INSTALLER_STAMP:-$(date +%Y%m%d-%H%M%S)}"
destination="$PWD/build/installer-$stamp"
[[ ! -e "$destination" ]] || { echo "Output already exists: $destination" >&2; exit 1; }
mkdir -p "$destination"
installer="$destination/安装国际音标输入法.app"
resources="$installer/Contents/Resources"
helper="$resources/Registration/RegisterInputSource.app"
mkdir -p "$installer/Contents/MacOS" "$resources/Payload" "$resources/Scripts" "$helper/Contents/MacOS"
ditto "$source_app" "$resources/Payload/PhoneticIPA.app"
cp scripts/install-macos.sh "$resources/Scripts/install-macos.sh"
cp "$source_app/Contents/Resources/AppIcon.icns" "$resources/Installer.icns"
chmod 755 "$resources/Scripts/install-macos.sh"
sdk=$(xcrun --sdk macosx --show-sdk-path)
xcrun clang -arch arm64 -arch x86_64 -mmacosx-version-min=13.0 -isysroot "$sdk" -fobjc-arc \
  macOS/RegisterInputSource.m -framework AppKit -framework Carbon -o "$helper/Contents/MacOS/RegisterInputSource"
for arch in arm64 x86_64; do
    xcrun swiftc -swift-version 5 -parse-as-library -O -target "$arch-apple-macos13.0" -sdk "$sdk" \
      macOS/Installer.swift -framework AppKit -o "$destination/Installer-$arch"
done
lipo -create "$destination/Installer-arm64" "$destination/Installer-x86_64" -output "$installer/Contents/MacOS/Installer"
python3 - "$installer" "$helper" "$version" <<'PY'
from pathlib import Path
import plistlib,sys
installer,helper=map(Path,sys.argv[1:3]);version=sys.argv[3]
base={'CFBundleDevelopmentRegion':'zh-Hans','CFBundleInfoDictionaryVersion':'6.0','CFBundlePackageType':'APPL','CFBundleShortVersionString':version,'CFBundleVersion':'8','LSMinimumSystemVersion':'13.0','NSPrincipalClass':'NSApplication','NSHighResolutionCapable':True}
for path,extra in [(installer,{'CFBundleIdentifier':'org.nephelium.phonetic-ipa.installer','CFBundleExecutable':'Installer','CFBundleName':'安装国际音标输入法','CFBundleDisplayName':'安装国际音标输入法','CFBundleIconFile':'Installer.icns','NSRequiresAquaSystemAppearance':False}), (helper,{'CFBundleIdentifier':'org.nephelium.phonetic-ipa.registration','CFBundleExecutable':'RegisterInputSource','CFBundleName':'PhoneticIPA Registration','LSUIElement':True})]:
    (path/'Contents/Info.plist').write_bytes(plistlib.dumps({**base,**extra},sort_keys=False))
PY
codesign --force --sign - "$helper"
codesign --force --sign - "$installer"
codesign --verify --deep --strict "$installer"
for executable in "$installer/Contents/MacOS/Installer" "$helper/Contents/MacOS/RegisterInputSource"; do
    # Some Apple lipo versions misparse multiple -verify_arch arguments.
    # Read the actual Mach-O slices and require exactly both supported CPUs.
    python3 - "$executable" <<'PY'
import subprocess,sys
path=sys.argv[1]
architectures=subprocess.check_output(['/usr/bin/lipo','-archs',path],text=True).split()
assert set(architectures)=={'arm64','x86_64'},(path,architectures)
print('Universal architectures verified:',architectures)
PY
    otool -L "$executable" | tee -a "$destination/dependencies.log"
    if otool -L "$executable" | grep -E '/Applications/Xcode|/Library/Developer|/opt/homebrew|/usr/local'; then
        echo 'Unexpected developer dependency in installer.' >&2; exit 1
    fi
done
python3 - "$installer" "$destination" <<'PY'
from pathlib import Path
import hashlib,json,sys
app,out=map(Path,sys.argv[1:])
manifest={p.relative_to(app).as_posix():{'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest(),'mode':p.stat().st_mode&0o777} for p in app.rglob('*') if p.is_file()}
(out/'installer-manifest.json').write_text(json.dumps(manifest,indent=2))
PY
printf 'Built double-click installer: %s\n' "$installer"
