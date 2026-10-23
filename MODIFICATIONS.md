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
