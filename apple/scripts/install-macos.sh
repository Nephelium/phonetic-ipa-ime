#!/bin/bash
# Run explicitly after the user authorizes installation of this input source.
set -euo pipefail
cd "$(dirname "$0")/.."
source_app="${IPA_MAC_APP:-$PWD/build/products/Debug/PhoneticIPA-Mac.app}"
target_app="$HOME/Library/Input Methods/PhoneticIPA.app"
[[ ! -L "$target_app" ]] || { echo "安装目标是符号链接，请先确认原有输入法的位置。未修改任何文件。" >&2; exit 1; }
registration_app="${IPA_REGISTRATION_APP:-$PWD/build/registration/RegisterInputSource.app}"
update=false
repair=false
no_open=false
for option in "$@"; do
    case "$option" in
        --update) update=true ;;
        --repair-enabled-list) repair=true ;;
        --no-open) no_open=true ;;
        *) echo "Usage: $0 [--update] [--repair-enabled-list] [--no-open]" >&2; exit 2 ;;
    esac
done
[[ -d "$source_app" ]] || { echo "安装内容不完整：缺少输入法应用。请重新下载并完整解压安装包。" >&2; exit 1; }
[[ -x "$registration_app/Contents/MacOS/RegisterInputSource" ]] || { echo "安装内容不完整：缺少预编译的注册助手。请重新下载并完整解压安装包，无需安装命令行工具。" >&2; exit 1; }
helper_id=$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$registration_app/Contents/Info.plist")
[[ "$helper_id" == org.nephelium.phonetic-ipa.registration ]] || { echo "Unexpected registration-helper bundle ID." >&2; exit 1; }
codesign --verify --deep --strict "$source_app"
codesign --verify --deep --strict "$registration_app"
if [[ -e "$target_app" && "$update" != true ]]; then
    echo "已安装国际音标输入法。请使用安装器的升级按钮，旧版本会保存在当前用户的 Application Support 中。" >&2; exit 1
fi
actual_id=$(/usr/libexec/PlistBuddy -c 'Print CFBundleIdentifier' "$source_app/Contents/Info.plist")
[[ "$actual_id" == org.nephelium.inputmethod.PhoneticIPA ]] || { echo "Unexpected bundle ID." >&2; exit 1; }
if [[ -e "$target_app" ]]; then
    [[ "$('/usr/libexec/PlistBuddy' -c 'Print CFBundleIdentifier' "$target_app/Contents/Info.plist")" == "$actual_id" ]] || { echo "Unexpected installed bundle ID." >&2; exit 1; }
fi
backup_root="${IPA_INSTALL_BACKUP_ROOT:-$HOME/Library/Application Support/PhoneticIPA/Install Backups}"
run_dir="$backup_root/$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$run_dir"
staged="$run_dir/staged.bundle"
ditto "$source_app" "$staged"
codesign --verify --deep --strict "$staged"

# Register the installed path directly. Unregistering a source copy that shares
# its bundle ID can invalidate the live catalogue during a repeated upgrade.
lsregister=/System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
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
    "$registration_app/Contents/MacOS/RegisterInputSource" --exchange-contents "$target_app/Contents" "$staged/Contents"
    mv "$staged/Contents" "$run_dir/previous.bundle/Contents"
else
    mv "$staged" "$target_app"
fi
codesign --verify --deep --strict "$target_app"
# Changing only Contents does not update the watched bundle/directory timestamps.
# Wake the input-source catalogue after replacing the app, including a version change.
touch "$target_app" "$HOME/Library/Input Methods"
"$lsregister" -f "$target_app"
# Establish the InputMethodKit connection in the login session before asking
# another process to inspect it. This does not open settings or select the IME.
open -g "$target_app"

# Run the precompiled helper in the GUI login session. No compiler is used here.
helper="$run_dir/RegisterInputSource.app"
ditto "$registration_app" "$helper"
codesign --verify --deep --strict "$helper"
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
    # A fresh process must inspect the catalogue in the same GUI login session.
    # A shell launched by SSH can have a separate, empty TIS catalogue.
    : > "$run_dir/independent-check.log"
    if open -g -W -n --stdout "$run_dir/independent-check.log" --stderr "$run_dir/independent-check-error.log" \
        "$helper" --args "$target_app" --check && grep -q '^ready=1$' "$run_dir/independent-check.log"; then
        ready=true
        break
    fi
    sleep 0.5
done
cat "$run_dir/independent-check.log"
[[ "$ready" == true ]] || { echo "Independent input-source verification failed. See $run_dir" >&2; exit 1; }
# Start the installed server in the existing GUI session without opening a window.
if [[ "$no_open" == true ]]; then
    : # The graphical installer offers an explicit Open Settings button.
elif [[ -e "$run_dir/previous.bundle" ]]; then
    open -g "$target_app"
else
    open "$target_app" --args --settings
fi
printf 'Installed and enabled: %s\nPrevious build and logs: %s\n' "$target_app" "$run_dir"
