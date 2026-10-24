#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
python3 scripts/verify-resources.py
default_source="$(pwd)/../msime-windows"
if [[ ! -d "$default_source/engine/ipa" ]]; then default_source="$(pwd)/.."; fi
source_root="${IPA_SOURCE_ROOT:-$default_source}"
stamp="${IPA_BUILD_STAMP:-$(date +%Y%m%d-%H%M%S)}"
destination="$(pwd)/build/device-$stamp"
if [[ -e "$destination" ]]; then echo "Output already exists: $destination" >&2; exit 1; fi
mkdir -p "$destination"
xcodebuild -project PhoneticIPA.xcodeproj -scheme PhoneticIPA-iOS -configuration Release \
  -destination 'generic/platform=iOS' -archivePath "$destination/PhoneticIPA.xcarchive" \
  -derivedDataPath "$destination/DerivedData" IPA_SOURCE_ROOT="$source_root" \
  CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO archive > "$destination/archive.log" 2>&1
app="$destination/PhoneticIPA.xcarchive/Products/Applications/PhoneticIPA-iOS.app"
extension="$app/PlugIns/PhoneticIPA-Keyboard.appex"
test -f "$app/PhoneticIPA-iOS"
test -f "$extension/PhoneticIPA-Keyboard"
xcrun lipo -verify_arch arm64 "$app/PhoneticIPA-iOS"
xcrun lipo -verify_arch arm64 "$extension/PhoneticIPA-Keyboard"
for executable in "$app/PhoneticIPA-iOS" "$extension/PhoneticIPA-Keyboard"; do
  xcrun vtool -show-build "$executable" | tee -a "$destination/platform-check.txt"
  xcrun vtool -show-build "$executable" | grep -qE 'platform[[:space:]]+IOS$'
done
mkdir "$destination/Payload"
ditto "$app" "$destination/Payload/PhoneticIPA-iOS.app"
version=$(/usr/libexec/PlistBuddy -c 'Print CFBundleShortVersionString' "$app/Info.plist")
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Invalid application version' >&2; exit 1; }
package="PhoneticIPA-iOS-$version-unsigned.ipa"
(cd "$destination" && /usr/bin/zip -qry "$package" Payload)
cp iOS/Shared.entitlements "$destination/Required-App-Group.entitlements"
shasum -a 256 "$destination/$package" | tee "$destination/SHA256.txt"
printf 'Unsigned device package: %s\n' "$destination"
