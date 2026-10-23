#!/bin/bash
# Run explicitly after the user authorizes installation of this input source.
set -euo pipefail
cd "$(dirname "$0")/.."
source_app="${IPA_MAC_APP:-$PWD/build/products/Debug/PhoneticIPA-Mac.app}"
target_app="$HOME/Library/Input Methods/PhoneticIPA.app"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode.app/Contents/Developer}"
update=false
repair=false
for option in "$@"; do
    case "$option" in
        --update) update=true ;;
        --repair-enabled-list) repair=true ;;
        *) echo "Usage: $0 [--update] [--repair-enabled-list]" >&2; exit 2 ;;
    esac
done
[[ -d "$source_app" ]] || { echo "Build the Mac application first." >&2; exit 1; }
if [[ -e "$target_app" && "$update" != true ]]; then
    echo "An installation exists. Use --update to preserve it in build/install-backups and install the new build." >&2; exit 1
fi
actual_id=$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$source_app/Contents/Info.plist")
[[ "$actual_id" == org.nephelium.inputmethod.PhoneticIPA ]] || { echo "Unexpected bundle ID." >&2; exit 1; }
if [[ -e "$target_app" ]]; then
    [[ "$('/usr/libexec/PlistBuddy' -c 'Print CFBundleIdentifier' "$target_app/Contents/Info.plist")" == "$actual_id" ]] || { echo "Unexpected installed bundle ID." >&2; exit 1; }
fi
run_dir="$PWD/build/install-backups/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$run_dir"
staged="$run_dir/staged.bundle"
ditto "$source_app" "$staged"
codesign --force --deep --sign - "$staged"
codesign --verify --deep --strict "$staged"

# Source and backup copies can win LaunchServices resolution for the same ID.
# Keep the installed copy as the only registered application for this build.
lsregister=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
unregister_copy() {
    local log="$run_dir/unregister-$(basename "$1").log"
    if "$lsregister" -u "$1" > "$log" 2>&1; then return 0; fi
    if grep -q 'failed to scan .*: -10814' "$log"; then
        printf 'No existing application registration: %s\n' "$1"
        return 0
    fi
    cat "$log" >&2
    return 1
}
unregister_copy "$source_app"
mkdir -p "$HOME/Library/Input Methods"
if [[ -e "$target_app" ]]; then
    executable="$target_app/Contents/MacOS/$('/usr/libexec/PlistBuddy' -c 'Print CFBundleExecutable' "$target_app/Contents/Info.plist")"
    while read -r pid command; do
        if [[ "$command" == "$executable" || "$command" == "$executable "* ]]; then
            kill -TERM "$pid"
            for attempt in {1..30}; do
                kill -0 "$pid" 2>/dev/null || break
                sleep 0.1
            done
            if kill -0 "$pid" 2>/dev/null; then
                echo 'Installed input method did not stop; leaving its files unchanged.' >&2
                exit 1
            fi
        fi
    done < <(ps -axo pid=,args=)
    # Retain the installed bundle directory's identity while replacing its contents.
    # Unregistering it first can make independent TIS clients lose it for minutes.
    mkdir -p "$run_dir/previous.bundle"
    mv "$target_app/Contents" "$run_dir/previous.bundle/Contents"
    if ! mv "$staged/Contents" "$target_app/Contents"; then
        mv "$run_dir/previous.bundle/Contents" "$target_app/Contents"
        exit 1
    fi
else
    mv "$staged" "$target_app"
fi
codesign --verify --deep --strict "$target_app"
"$lsregister" -f "$target_app"

# Run registration in the GUI login session without activating a window.
helper="$run_dir/RegisterInputSource.app"
mkdir -p "$helper/Contents/MacOS"
xcrun clang -fobjc-arc macOS/RegisterInputSource.m -framework AppKit -framework Carbon -o "$helper/Contents/MacOS/RegisterInputSource"
cat > "$helper/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?><!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"><plist version="1.0"><dict><key>CFBundleIdentifier</key><string>org.nephelium.phonetic-ipa.registration</string><key>CFBundleExecutable</key><string>RegisterInputSource</string><key>CFBundlePackageType</key><string>APPL</string><key>LSUIElement</key><true/><key>NSPrincipalClass</key><string>NSApplication</string></dict></plist>
PLIST
arguments=("$target_app")
if [[ "$repair" == true ]]; then arguments+=(--repair-enabled-list "$run_dir/enabled-sources-before.plist"); fi
open -g -W -n --stdout "$run_dir/registration.log" --stderr "$run_dir/registration-error.log" "$helper" --args "${arguments[@]}"
cat "$run_dir/registration.log" "$run_dir/registration-error.log"
if ! grep -q '^registration-complete=1$' "$run_dir/registration.log"; then
    printf 'Installed, but input source enablement is unverified. See %s\n' "$run_dir" >&2
    printf 'If macOS did not persist enablement, rerun with --update --repair-enabled-list to back up the list and append only this input method.\n' >&2
    exit 1
fi
ready=false
for attempt in {1..6}; do
    if "$helper/Contents/MacOS/RegisterInputSource" "$target_app" --check > "$run_dir/independent-check.log"; then
        ready=true
        break
    fi
    sleep 0.5
done
cat "$run_dir/independent-check.log"
[[ "$ready" == true ]] || { echo "Independent input-source verification failed. See $run_dir" >&2; exit 1; }
# Start the installed server in the existing GUI session without opening a window.
open -g "$target_app"
printf 'Installed and enabled: %s\nPrevious build and logs: %s\n' "$target_app" "$run_dir"
