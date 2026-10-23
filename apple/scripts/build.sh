#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
default_source="$(pwd)/../msime-windows"
if [[ ! -d "$default_source/engine/ipa" ]]; then default_source="$(pwd)/.."; fi
source_root="${IPA_SOURCE_ROOT:-$default_source}"
mkdir -p build
xcrun swift scripts/menu-icon.swift Generated/IPA/MenuIcon.tiff
xcrun clang++ -std=c++17 -O2 -fobjc-arc -I"$source_root" -IShared Shared/IPAEngine.mm Tests/CoreTests.mm -framework Foundation -lsqlite3 -o build/apple-core-tests
/usr/bin/time -l build/apple-core-tests Generated/IPA > build/core-tests.log 2>&1
xcodebuild -project PhoneticIPA.xcodeproj -target PhoneticIPA-Mac -configuration Debug IPA_SOURCE_ROOT="$source_root" SYMROOT="$(pwd)/build/products" OBJROOT="$(pwd)/build/intermediates" CODE_SIGNING_ALLOWED=NO build > build/macos-build.log 2>&1
xcodebuild -project PhoneticIPA.xcodeproj -target PhoneticIPA-iOS -configuration Debug -sdk iphonesimulator IPA_SOURCE_ROOT="$source_root" SYMROOT="$(pwd)/build/products" OBJROOT="$(pwd)/build/intermediates" CODE_SIGNING_ALLOWED=NO build > build/ios-build.log 2>&1
printf 'Mac and iOS Simulator products built successfully.\n'
