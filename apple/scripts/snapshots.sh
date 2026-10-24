#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
default_source="$(pwd)/../msime-windows"
if [[ ! -d "$default_source/engine/ipa" ]]; then default_source="$(pwd)/.."; fi
source_root="${IPA_SOURCE_ROOT:-$default_source}"
snapshot_app="$PWD/build/Snapshots.app"
mkdir -p "$snapshot_app/Contents/MacOS" "$snapshot_app/Contents/Resources" build/previews
if [[ ! -e "$snapshot_app/Contents/Resources/IPA" ]]; then ln -s "$PWD/Generated/IPA" "$snapshot_app/Contents/Resources/IPA"; fi
cat > "$snapshot_app/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>CFBundleIdentifier</key><string>org.nephelium.phonetic-ipa.snapshots</string><key>CFBundleExecutable</key><string>Snapshots</string><key>CFBundlePackageType</key><string>APPL</string></dict></plist>
PLIST
xcrun clang++ -std=c++17 -O2 -fobjc-arc -I"$source_root" -IShared -c Shared/IPAEngine.mm -o build/snapshot-engine.o
xcrun swiftc -swift-version 5 -parse-as-library -import-objc-header Shared/IPAEngine.h Shared/InputModel.swift Shared/Updates.swift Shared/KeyboardViews.swift Tests/Snapshots.swift build/snapshot-engine.o -framework AppKit -framework SwiftUI -lc++ -lsqlite3 -o "$snapshot_app/Contents/MacOS/Snapshots"
IPA_UPDATE_DISABLE_NETWORK=1 "$snapshot_app/Contents/MacOS/Snapshots" "$PWD/build/previews" > build/session-tests.log 2>&1
cat build/session-tests.log
