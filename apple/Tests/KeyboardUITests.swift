import XCTest

final class KeyboardUITests: XCTestCase {
    func testRealKeyboardCommit() throws {
        continueAfterFailure = false
        let app = XCUIApplication()
        app.launchEnvironment["IPA_UPDATE_DISABLE_NETWORK"] = "1"
        app.launch()
        if app.buttons["onboarding-next"].waitForExistence(timeout: 10) {
            for page in 1...4 {
                let title = ["先认清当前输入状态", "用拼音输入音标或汉字", "切换到直接音标键盘", "输入声调，查找更多符号"][page - 1]
                XCTAssertTrue(app.staticTexts[title].exists)
                let shot = XCTAttachment(screenshot: app.screenshot()); shot.name = "First use guide \(page)"; shot.lifetime = .keepAlways; add(shot)
                app.scrollViews["onboarding-content"].swipeUp()
                let bottom = XCTAttachment(screenshot: app.screenshot()); bottom.name = "First use guide bottom \(page)"; bottom.lifetime = .keepAlways; add(bottom)
                app.buttons["onboarding-next"].tap()
            }
        }
        XCTAssertTrue(app.buttons["check-for-update"].waitForExistence(timeout: 15))
        let settings = XCUIApplication(bundleIdentifier: "com.apple.Preferences")
        settings.launch()
        print("SETTINGS_INITIAL\n" + settings.debugDescription)
        let general = settings.buttons["com.apple.settings.general"]
        for _ in 0..<6 where !general.isHittable { settings.swipeUp() }
        XCTAssertTrue(general.waitForExistence(timeout: 10)); general.tap()
        let keyboard = settings.staticTexts.matching(NSPredicate(format: "label IN %@", ["Keyboard", "键盘"])).firstMatch
        for _ in 0..<6 where !keyboard.isHittable { settings.swipeUp() }
        XCTAssertTrue(keyboard.waitForExistence(timeout: 10)); keyboard.tap()
        print("KEYBOARD_SETTINGS\n" + settings.debugDescription)
        settings.cells["KEYBOARDS"].tap()
        print("INSTALLED_KEYBOARDS\n" + settings.debugDescription)
        let existing = settings.descendants(matching: .any).matching(NSPredicate(format: "label == %@", "国际音标输入法")).firstMatch
        let addKeyboard = settings.descendants(matching: .any).matching(NSPredicate(format: "label BEGINSWITH 'Add New Keyboard' OR label BEGINSWITH '添加新键盘'")).firstMatch
        if !existing.exists {
        XCTAssertTrue(addKeyboard.waitForExistence(timeout: 10)); addKeyboard.tap()
        print("KEYBOARD_LIST\n" + settings.debugDescription)
        let custom = settings.staticTexts["国际音标输入法"].firstMatch
        XCTAssertTrue(custom.waitForExistence(timeout: 10)); custom.tap()
        }
        app.activate()
        XCTAssertTrue(app.tabBars.buttons["试打"].waitForExistence(timeout: 10))
        app.tabBars.buttons["试打"].tap()
        print("TRIAL_TAB\n" + app.debugDescription)
        let area = app.textViews["typing-area"]
        XCTAssertTrue(area.waitForExistence(timeout: 10)); area.tap()
        // A newly installed keyboard extension starts asynchronously after its host receives focus.
        _ = app.buttons["key-w"].waitForExistence(timeout: 30)
        print("INITIAL_KEYBOARD\n" + app.debugDescription)
        if !app.buttons["function-拼音"].exists && !app.buttons["function-IPA"].exists {
            let globe = app.buttons.matching(NSPredicate(format: "label CONTAINS[c] 'keyboard' OR label CONTAINS '键盘'")).firstMatch
            XCTAssertTrue(globe.waitForExistence(timeout: 10)); globe.press(forDuration: 1)
            let keyboardName = app.staticTexts["国际音标输入法"].firstMatch
            XCTAssertTrue(keyboardName.waitForExistence(timeout: 10)); keyboardName.tap()
        }
        if app.buttons["function-IPA"].exists { app.buttons["function-IPA"].tap() }
        XCTAssertTrue(app.buttons["function-拼音"].waitForExistence(timeout: 10))
        if !app.buttons["key-w"].isHittable { area.tap() }
        let visibleKeyboard = XCTNSPredicateExpectation(predicate: NSPredicate(format: "hittable == true"), object: app.buttons["key-w"])
        let visibleResult = XCTWaiter.wait(for: [visibleKeyboard], timeout: 15)
        let initialShot = XCTAttachment(screenshot: app.screenshot())
        initialShot.name = "Initial keyboard geometry"; initialShot.lifetime = .keepAlways; add(initialShot)
        XCTAssertEqual(visibleResult, .completed, "The software keyboard must be visible before entering text")
        defer { XCUIDevice.shared.orientation = .portrait }
        XCUIDevice.shared.orientation = .landscapeLeft
        let landscape = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in
            app.windows.firstMatch.frame.width > app.windows.firstMatch.frame.height
        }, object: nil)
        XCTAssertEqual(XCTWaiter.wait(for: [landscape], timeout: 10), .completed)
        if !app.buttons["key-w"].isHittable { area.tap() }
        let landscapeKeys = XCTNSPredicateExpectation(predicate: NSPredicate(format: "hittable == true"), object: app.buttons["keyboard-help"])
        let landscapeResult = XCTWaiter.wait(for: [landscapeKeys], timeout: 10)
        print("LANDSCAPE_UI\n" + app.debugDescription)
        XCTAssertEqual(landscapeResult, .completed)
        XCTAssertTrue(app.buttons["key-w"].isHittable)
        let keyAreaHeight = app.buttons["keyboard-help"].frame.maxY - app.buttons["digit-1"].frame.minY
        XCTAssertLessThan(keyAreaHeight, app.windows.firstMatch.frame.height * 0.7, "Landscape keys occupy too much of the host")
        let landscapeShot = XCTAttachment(screenshot: app.screenshot()); landscapeShot.name = "Landscape keyboard geometry"; landscapeShot.lifetime = .keepAlways; add(landscapeShot)
        XCUIDevice.shared.orientation = .portrait
        let portrait = XCTNSPredicateExpectation(predicate: NSPredicate { _, _ in
            app.windows.firstMatch.frame.height > app.windows.firstMatch.frame.width
        }, object: nil)
        XCTAssertEqual(XCTWaiter.wait(for: [portrait], timeout: 10), .completed)
        if !app.buttons["key-w"].isHittable { area.tap() }
        app.buttons["key-w"].tap(); app.buttons["key-u"].tap()
        let hanzi = app.buttons["hanzi-恶-wu"]
        for _ in 0..<30 where !hanzi.exists { app.buttons["›"].tap() }
        XCTAssertTrue(hanzi.waitForExistence(timeout: 5)); hanzi.tap()
        XCTAssertEqual(area.value as? String, "恶")
        app.buttons["key-w"].tap(); app.buttons["key-u"].tap()
        let ipa = app.buttons["ipa-恶-wu"]
        for _ in 0..<30 where !ipa.exists { app.buttons["›"].tap() }
        XCTAssertTrue(ipa.waitForExistence(timeout: 5)); ipa.tap()
        XCTAssertFalse(app.buttons["reading-e3"].exists)
        XCTAssertEqual(area.value as? String, "恶", "Opening readings must not commit")
        app.buttons["reading-wu4"].tap()
        XCTAssertFalse(app.buttons["reading-wu4"].exists, "The completed reading chooser must close")
        XCTAssertEqual(area.value as? String, "恶wu˥˩")
        app.buttons["function-拼音"].tap(); app.buttons["key-a"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩", "A short tap must only open the symbol picker")
        let symbol = app.buttons["symbol-choice-a-0"]
        XCTAssertTrue(symbol.waitForExistence(timeout: 5)); symbol.tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a")
        for digit in ["2", "1", "4"] {
            let key = app.buttons["digit-" + digit]
            key.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: 0.8)).press(forDuration: 0.05, thenDragTo: key.coordinate(withNormalizedOffset: CGVector(dx: 0.5, dy: -0.4)))
        }
        app.buttons["hanzi-˨˩˦-"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦")
        app.buttons["digit-6"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6", "Digit tap with no candidates must type a number")
        let screenshot = XCTAttachment(screenshot: app.screenshot())
        screenshot.name = "Actual keyboard Hanzi IPA CIN commits"; screenshot.lifetime = .keepAlways
        add(screenshot)
        XCTAssertFalse(app.buttons["声调"].exists)
        app.buttons["keyboard-help"].tap()
        XCTAssertTrue(app.staticTexts["键盘使用说明"].waitForExistence(timeout: 5))
        let guide = app.scrollViews["usage-guide"]
        XCTAssertTrue(guide.waitForExistence(timeout: 5))
        guide.swipeUp()
        let helpShot = XCTAttachment(screenshot: app.screenshot())
        helpShot.name = "Keyboard usage guide"; helpShot.lifetime = .keepAlways; add(helpShot)
        app.buttons["help-close"].tap()
        XCTAssertTrue(app.buttons["digit-2"].exists)
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6", "Help must not change the host text")
        app.buttons["function-符号"].tap()
        app.buttons["key-\\"].tap(); app.buttons["symbol-choice-\\-0"].tap()
        app.buttons["key-/"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/", "Punctuation codes must remain reachable")
        app.buttons["function-ABC"].tap()
        app.buttons["key-g"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/", "Opening g must not insert a code letter")
        let gShot = XCTAttachment(screenshot: app.screenshot()); gShot.name = "Short tap g symbol picker"; gShot.lifetime = .keepAlways; add(gShot)
        app.buttons["symbol-picker-close"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/", "Cancel must not edit the host")
        app.buttons["key-g"].press(forDuration: 0.7)
        XCTAssertTrue(app.buttons["symbol-choice-g-1"].waitForExistence(timeout: 5)); app.buttons["symbol-choice-g-1"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/ɠ", "Long press must commit the selected IPA exactly once")
        app.buttons["key-v"].tap()
        app.buttons["symbol-code-voqs"].tap()
        let lastCombo = app.buttons["symbol-choice-voqs-57"]
        let picker = app.scrollViews["symbol-picker-list"]
        for _ in 0..<15 where !lastCombo.isHittable { picker.swipeUp() }
        XCTAssertTrue(lastCombo.isHittable, "The last combination must be touchable after scrolling")
        let comboShot = XCTAttachment(screenshot: app.screenshot()); comboShot.name = "VoQS combination picker scrolled"; comboShot.lifetime = .keepAlways; add(comboShot)
        lastCombo.tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/ɠΘ")
        app.buttons["digit-0"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/ɠΘ∅")
        app.buttons["keyboard-plus"].tap()
        app.buttons["plus-ipa-pulmonic-001"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/ɠΘ∅p")
        app.buttons["extIPA"].tap(); app.buttons["plus-extipa-consonants-001"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/ɠΘ∅pp̪")
        let keyboardPlusShot = XCTAttachment(screenshot: app.screenshot()); keyboardPlusShot.name = "Keyboard inline IPA Plus"; keyboardPlusShot.lifetime = .keepAlways; add(keyboardPlusShot)
        app.buttons["VoQS"].tap(); app.buttons["plus-close"].tap()
        app.buttons["function-IPA"].tap()
        let base = area.value as? String ?? ""
        for c in "yanwu" { app.buttons["key-" + String(c)].tap() }
        app.buttons["digit-1"].tap()
        XCTAssertEqual(area.value as? String, base + "jan˥˩ wu˥˩", "Default digit selection must commit IPA")
        app.buttons["output-mode"].tap()
        for c in "yanwu" { app.buttons["key-" + String(c)].tap() }
        app.buttons["digit-1"].tap()
        XCTAssertEqual(area.value as? String, base + "jan˥˩ wu˥˩厌恶", "Hanzi output switch must affect digits")
        app.buttons["output-mode"].tap()
        app.buttons["transcription-rules"].tap()
        print("RULE_MENU\n" + app.debugDescription)
        let narrow = app.descendants(matching: .any).matching(NSPredicate(format: "label == %@", "UntPhesoca 严")).firstMatch
        for _ in 0..<6 where !narrow.isHittable { app.scrollViews["transcription-rule-list"].swipeUp() }
        XCTAssertTrue(narrow.isHittable, "Last transcription standard must be reachable by scrolling"); narrow.tap()
        for c in "yanwu" { app.buttons["key-" + String(c)].tap() }
        app.buttons["digit-1"].tap()
        XCTAssertEqual(area.value as? String, base + "jan˥˩ wu˥˩厌恶jɛ̠n̚˥˩ uᵝ˥˩", "Rule menu must change committed IPA")
        app.buttons["transcription-rules"].tap()
        let beijing = app.buttons["rule-beijing"]
        for _ in 0..<6 where !beijing.isHittable { app.scrollViews["transcription-rule-list"].swipeDown() }
        XCTAssertTrue(beijing.isHittable); beijing.tap()
        let ruleShot = XCTAttachment(screenshot: app.screenshot()); ruleShot.name = "Default IPA and quick transcription rules"; ruleShot.lifetime = .keepAlways; add(ruleShot)
        app.buttons["dismiss-trial-keyboard"].tap()
        area.tap()
        XCTAssertTrue(app.buttons["key-g"].waitForExistence(timeout: 15), "Keyboard must reopen after the rule picker closes")
        let beforeReopenInput = area.value as? String ?? ""
        app.buttons["digit-6"].tap()
        let afterReopenInput = area.value as? String ?? ""
        XCTAssertEqual(afterReopenInput.filter { $0 == "6" }.count, beforeReopenInput.filter { $0 == "6" }.count + 1, "Reopened keyboard must insert exactly one digit")
        XCTAssertEqual(afterReopenInput.replacingOccurrences(of: "6", with: ""), beforeReopenInput.replacingOccurrences(of: "6", with: ""), "Reopening must preserve every existing Unicode character")

        app.buttons["dismiss-trial-keyboard"].tap()
        app.tabBars.buttons["设置"].tap()
        let plus = app.buttons["IPA Plus 选音标"]
        for _ in 0..<10 where !plus.isHittable { app.scrollViews.firstMatch.swipeUp() }
        XCTAssertTrue(plus.isHittable); plus.tap()
        XCTAssertTrue(app.buttons["plus-ipa-pulmonic-001"].waitForExistence(timeout: 5))
        let chart = app.scrollViews["plus-matrix-extended"]
        XCTAssertTrue(chart.isHittable)
        let plusShot = XCTAttachment(screenshot: app.screenshot()); plusShot.name = "IPA Plus phone matrix"; plusShot.lifetime = .keepAlways; add(plusShot)
        app.buttons["plus-ipa-pulmonic-001"].tap()
        for _ in 0..<6 where !app.buttons["plus-ipa-pulmonic-013"].isHittable { chart.swipeLeft() }
        XCTAssertTrue(app.buttons["plus-ipa-pulmonic-013"].isHittable); app.buttons["plus-ipa-pulmonic-013"].tap()
        let rightShot = XCTAttachment(screenshot: app.screenshot()); rightShot.name = "IPA Plus right columns"; rightShot.lifetime = .keepAlways; add(rightShot)
        app.buttons["extIPA"].tap()
        XCTAssertTrue(app.buttons["plus-extipa-consonants-001"].waitForExistence(timeout: 5))
        app.buttons["字号 ＋"].tap()
        let extShot = XCTAttachment(screenshot: app.screenshot()); extShot.name = "extIPA matrix larger text"; extShot.lifetime = .keepAlways; add(extShot)
        app.buttons["VoQS"].tap()
        let voqsShot = XCTAttachment(screenshot: app.screenshot()); voqsShot.name = "VoQS adaptive phone grid"; voqsShot.lifetime = .keepAlways; add(voqsShot)
        app.buttons["plus-close"].tap()
        let modifierPreview = app.staticTexts["output-preview-6"]
        for _ in 0..<8 where !modifierPreview.isHittable { app.scrollViews.firstMatch.swipeDown() }
        XCTAssertTrue(modifierPreview.isHittable, "The modifier-letter candidate must have a visible preview")
        let editorShot = XCTAttachment(screenshot: app.screenshot()); editorShot.name = "Editor modifier letter preview"; editorShot.lifetime = .keepAlways; add(editorShot)

    }
}
