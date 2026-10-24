# Modifications by Nephelium

Date: 2026-10-23. Upstream: metasequoiaime/msime-windows v0.11.1, commit dd018f79858314ed542a47c528608752b13bd261.

Independent phonetic IPA prototype: TSF identity and IPC isolation, Hanzi/IPA commits, CIN keymap editor, IPA Plus charts, tone shortcuts, word-level pronunciation choices, Doulos SIL display and corresponding tests. Original copyright notices and component licenses are retained.

The following upstream paths are changed or added in this derivative:

- `engine/contracts/input_state_shortcuts.h`
- `engine/contracts/ipa_input.h`
- `engine/contracts/tests/input_state_shortcuts.json`
- `engine/contracts/windows_ipc.h`
- `engine/core/data_path.h`
- `engine/core/input_session_composition.cpp`
- `engine/ipa/ipa_composer.h`
- `engine/ipa/ipa_readings.h`
- `engine/ipa/ipa_tables.h`
- `installer/default_config/config.default.toml`
- `server/CMakeLists.txt`
- `server/MetasequoiaImeServer.manifest`
- `server/assets/config/config.toml`
- `server/src/config/ime_config.cpp`
- `server/src/global/globals.h`
- `server/src/ipa/ipa_keyboard_layout.h`
- `server/src/ipa/ipa_profile.h`
- `server/src/ipa/ipa_reading_session.h`
- `server/src/ipa/ipa_runtime.h`
- `server/src/ipc/event_listener.cpp`
- `server/src/ipc/event_listener.h`
- `server/src/ipc/event_listener_candidates.cpp`
- `server/src/ipc/event_listener_internal.h`
- `server/src/ipc/event_listener_keys.cpp`
- `server/src/ipc/event_listener_selection.cpp`
- `server/src/ipc/event_listener_worker.cpp`
- `server/src/main.cpp`
- `server/src/session/engine_input_session.h`
- `server/src/session/input_session.h`
- `server/src/settings/game_compat_name.cpp`
- `server/src/settings/native/native_ipa_candidates.h`
- `server/src/settings/native/native_ipa_keyboard.h`
- `server/src/settings/native/native_ipa_plus.h`
- `server/src/settings/native/native_page_ipa.cpp`
- `server/src/settings/native/native_settings_bindings.cpp`
- `server/src/settings/native/native_settings_controls.cpp`
- `server/src/settings/native/native_settings_controls.h`
- `server/src/settings/native/native_settings_pages.cpp`
- `server/src/settings/native/native_settings_pages.h`
- `server/src/settings/native/native_settings_window.cpp`
- `server/src/settings/settings_app.cpp`
- `server/src/settings/settings_launcher.cpp`
- `server/src/utils/ime_paths.cpp`
- `server/src/watchdog/main.cpp`
- `server/src/window/candidate_presenter.cpp`
- `server/src/window/candidate_skin_scene.cpp`
- `server/src/window/candidate_skin_scene.h`
- `server/src/window/candidate_view_model.h`
- `server/src/window/floating_toolbar_presenter.cpp`
- `server/tests/src/native_font_checks.h`
- `server/tests/src/native_preview_capture.h`
- `server/tests/src/test_config_template_merge.cpp`
- `server/tests/src/test_ipa_native.cpp`
- `server/tests/src/test_ipa_profile.cpp`
- `server/tests/src/test_ipa_prototype.cpp`
- `server/tests/src/test_ipa_readings.cpp`
- `server/tests/src/test_ipa_session.cpp`
- `server/tests/src/test_ipc_protocol_constants.cpp`
- `server/vcpkg-configuration.json`
- `skins/ipa-celadon/skin.toml`
- `skins/ipa-night/skin.toml`
- `skins/ipa-paper/skin.toml`
- `tests/pipe-probe/ipa_readings_probe.h`
- `tests/pipe-probe/main.cpp`
- `ui/include/msimeui/Controls.h`
- `ui/include/msimeui/Fonts.h`
- `ui/src/CandidateList.cpp`
- `ui/src/ControlsInternal.cpp`
- `ui/src/Fonts.cpp`
- `ui/src/tsf/TextLayout.cpp`
- `ui/tests/CMakeLists.txt`
- `ui/tests/src/test_candidate_fonts.cpp`
- `ui/tests/src/test_layout.cpp`
- `windows/CMakeLists.txt`
- `windows/src/Composition/CompositionProcessorEngine.cpp`
- `windows/src/Global/GameCompat.h`
- `windows/src/Global/Globals.cpp`
- `windows/src/IME/MetasequoiaIME.cpp`
- `windows/src/IME/MetasequoiaIME.def`
- `windows/src/Key/KeyEventSink.cpp`
- `windows/src/Key/KeyEventSink_DeferredKeys.cpp`
- `windows/src/Register/Register.cpp`
- `windows/src/Utils/FanyUtils.cpp`
- `windows/src/Utils/FanyUtils.h`
- `windows/tests/input_state_shortcuts.cpp`
- `windows/vcpkg-configuration.json`

Additional prototype resources, build/launch scripts, attribution and verification notes are in `prototype/`. Root README introduces this derivative; `README.upstream.md` preserves the original upstream README. No upstream CI workflow is modified.

## 2026-10-23 Apple and Android prototype

- Added `apple/` with an InputMethodKit host, iOS App and keyboard extension, shared Objective-C++ bridge, Swift session, visual editor and structured IPA Plus picker.
- Added `android/` with InputMethodService, native JNI bridge, keyboard/editor/help and real-host instrumentation.
- Added digit-row swipe tones, manual polyphone selection, punctuation-code access, separate Hanzi/IPA actions and Doulos SIL display.
- Extended `engine/ipa/ipa_readings.h` to accept word readings supplied by a platform SQLite index; existing Windows conversion remains compatible. Updated its regression tests.
- Added an original generated bird icon under `assets/icons/`, including source prompt, date and checksum. Platform icon assets are size derivatives.
- Included reproducible data preparation, local unsigned iOS packaging, native Android build entry points and platform-specific verification notes. Large generated databases, personal settings, signing identities and build caches are excluded.
- macOS builds, but real host commit remains unverified because selecting the registered input source returns -50. iOS and Android passed actual keyboard input on their dedicated simulators; physical device testing remains outstanding.

Original upstream history, GPLv3 license, font/data licenses and `THIRD_PARTY_NOTICES.txt` are retained. No upstream CI workflow was changed.

## 2026-10-23 macOS input source repair

- Added parent-before-mode registration, explicit enabled-list repair with a private backup, idempotent preservation checks and independent-process verification.
- Updated installation without discarding the installed outer bundle directory; retained previous contents and limited process management to this product.
- Prevented candidate windows belonging to background clients from covering the active application.
- Replaced the host probe with a full AppKit event-loop test using Quartz-backed events directed only to its own NSTextView. Verified marked text, separate Hanzi/IPA commits, word readings, manual ambiguity selection, CIN, Shift tones, punctuation and restoration of the prior application/input source.
- The earlier -50 limitation above is superseded by the dedicated macOS repair record. Other desktop hosts and physical mobile devices retain their previous verification boundaries.

References and investigation sources are listed in `docs/testing/2026-10-23-Mac输入源修复与真实输入验证.md`; no third-party implementation was copied for this repair.

## 2026-10-23 · 0.1.1 双来源更新与发布

新增 `engine/ipa/update_manifest.h`、Windows `utils/ipa_update.*`、Apple `Shared/Updates.swift`、Android `Updates.java`，在设置与后台启动入口接入官网/GitHub 检测。Android 仅为公开版本信息增加 INTERNET 权限。共享更新向量覆盖版本排序、来源独立性与下载地址限制。

更新 Windows 关于页产品名和下载入口，保留水杉与依赖来源。Mac 安装脚本刷新应用和输入法目录时间戳，修复更新版本后系统输入源目录未刷新的情况。Mac 实际宿主回归通过并恢复原焦点。新增四平台使用说明、官网可展开下载组件、测试和发布记录。用户码表、学习库、凭据、私有系统偏好与大构建产物不进入源码。

Windows 包内附带 Microsoft Visual C++ 14.44.35112 的未修改可再分发运行库，按微软 Visual Studio 许可分发，独立于本项目 GPL。原有字体、CC-CEDICT 与 IPA Plus 资料许可保持。
