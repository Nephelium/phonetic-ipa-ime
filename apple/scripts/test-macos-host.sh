#!/bin/bash
# Requires explicit authorization to register and enable the installed IME.
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
test_app="$PWD/build/MacHostTests.app"
mkdir -p "$test_app/Contents/MacOS" build/previews
cat > "$test_app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>CFBundleIdentifier</key><string>org.nephelium.phonetic-ipa.hosttest</string><key>CFBundleExecutable</key><string>MacHostTests</string><key>CFBundlePackageType</key><string>APPL</string><key>LSUIElement</key><true/><key>NSPrincipalClass</key><string>NSApplication</string></dict></plist>
PLIST
xcrun clang -fobjc-arc Tests/MacHostTests.m -framework AppKit -framework Carbon -o "$test_app/Contents/MacOS/MacHostTests"
test -x "$test_app/Contents/MacOS/MacHostTests"
run_id="$(date +%Y%m%d-%H%M%S)"
log="$PWD/build/mac-host-$run_id.log"
open -W -n "$test_app" --args "$PWD/build/previews/mac-actual-host.png" "$log"
cat "$log"
grep -q 'Mac host: marked text, Hanzi, IPA, CIN and Shift tones passed' "$log"
