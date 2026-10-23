#!/bin/bash
# Run explicitly after the user authorizes installation of this input source.
set -euo pipefail
cd "$(dirname "$0")/.."
source_app="$PWD/build/products/Debug/PhoneticIPA-Mac.app"
target_app="$HOME/Library/Input Methods/PhoneticIPA.app"
[[ -d "$source_app" ]] || { echo "Build the Mac application first." >&2; exit 1; }
[[ ! -e "$target_app" ]] || { echo "An installation already exists at $target_app. Review it before updating." >&2; exit 1; }
actual_id=$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$source_app/Contents/Info.plist")
[[ "$actual_id" == org.nephelium.inputmethod.PhoneticIPA ]] || { echo "Unexpected bundle ID." >&2; exit 1; }
mkdir -p "$HOME/Library/Input Methods"
ditto "$source_app" "$target_app"
codesign --force --deep --sign - "$target_app"
codesign --verify --deep --strict "$target_app"
printf 'Installed: %s\nAdd 国际音标输入法 in System Settings → Keyboard → Input Sources.\n' "$target_app"
