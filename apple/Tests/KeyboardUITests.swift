import XCTest

final class KeyboardUITests: XCTestCase {
    func testRealKeyboardCommit() throws {
        continueAfterFailure = false
        let app = XCUIApplication()
        app.launchEnvironment["IPA_UPDATE_DISABLE_NETWORK"] = "1"
        app.launch()
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
        if !app.buttons["function-中"].exists && !app.buttons["function-IPA"].exists {
            let globe = app.buttons.matching(NSPredicate(format: "label CONTAINS[c] 'keyboard' OR label CONTAINS '键盘'")).firstMatch
            XCTAssertTrue(globe.waitForExistence(timeout: 10)); globe.press(forDuration: 1)
            let keyboardName = app.staticTexts["国际音标输入法"].firstMatch
            XCTAssertTrue(keyboardName.waitForExistence(timeout: 10)); keyboardName.tap()
        }
        if app.buttons["function-IPA"].exists { app.buttons["function-IPA"].tap() }
        XCTAssertTrue(app.buttons["function-中"].waitForExistence(timeout: 10))
        if !app.buttons["key-w"].isHittable { area.tap() }
        let visibleKeyboard = XCTNSPredicateExpectation(predicate: NSPredicate(format: "hittable == true"), object: app.buttons["key-w"])
        let visibleResult = XCTWaiter.wait(for: [visibleKeyboard], timeout: 15)
        let initialShot = XCTAttachment(screenshot: app.screenshot())
        initialShot.name = "Initial keyboard geometry"; initialShot.lifetime = .keepAlways; add(initialShot)
        XCTAssertEqual(visibleResult, .completed, "The software keyboard must be visible before entering text")
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
        XCTAssertEqual(area.value as? String, "恶", "Choosing a reading must not commit")
        ipa.tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩")
        app.buttons["function-中"].tap(); app.buttons["key-a"].tap()
        let symbol = app.buttons["hanzi-a-"]
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
        app.buttons["key-\\"].tap(); app.buttons["hanzi-˥-"].tap()
        app.buttons["key-/"].tap(); app.buttons["hanzi-/-"].tap()
        XCTAssertEqual(area.value as? String, "恶wu˥˩a˨˩˦6˥/", "Punctuation codes must remain reachable")
    }
}
