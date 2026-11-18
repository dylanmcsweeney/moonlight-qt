#include <QtTest>
#include <Limelight.h>

#include <algorithm>
#include <tuple>

#include "streaming/input/input.h"

namespace {

struct KeyboardEvent {
    quint16 code;
    char action;
    char modifiers;
    char flags;
};

QList<KeyboardEvent> sentEvents;

void sendKey(SdlInputHandler& input, SDL_Scancode scancode, Uint8 state, Uint16 modifiers = KMOD_NONE)
{
    SDL_KeyboardEvent event = {};
    event.type = state == SDL_PRESSED ? SDL_KEYDOWN : SDL_KEYUP;
    event.state = state;
    event.keysym.scancode = scancode;
    event.keysym.mod = modifiers;
    input.handleKeyEvent(&event);
}

void compareKey(const KeyboardEvent& event, quint16 code, char action, char modifiers, char flags = 0)
{
    QCOMPARE(event.code, code);
    QCOMPARE(event.action, action);
    QCOMPARE(event.modifiers, modifiers);
    QCOMPARE(event.flags, flags);
}

} // namespace

int LiSendKeyboardEvent2(short code, char action, char modifiers, char flags)
{
    sentEvents.append({static_cast<quint16>(code), action, modifiers, flags});
    return 0;
}

class KeyboardTest : public QObject {
    Q_OBJECT

private slots:
    void init() { sentEvents.clear(); }

    void releasingOneEnterLeavesTheOtherDown_data()
    {
        QTest::addColumn<bool>("releaseKeypad");
        QTest::newRow("main Enter released first") << false;
        QTest::newRow("keypad Enter released first") << true;
    }

    void releasingOneEnterLeavesTheOtherDown()
    {
        QFETCH(bool, releaseKeypad);
        StreamingPreferences preferences;
        SdlInputHandler input(preferences, 1920, 1080);
        sendKey(input, SDL_SCANCODE_RETURN, SDL_PRESSED);
        sendKey(input, SDL_SCANCODE_KP_ENTER, SDL_PRESSED, KMOD_LSHIFT);
        sendKey(input, releaseKeypad ? SDL_SCANCODE_KP_ENTER : SDL_SCANCODE_RETURN, SDL_RELEASED);
        input.raiseAllKeys();

        QCOMPARE(sentEvents.size(), 4);
        compareKey(sentEvents[0], 0x800D, KEY_ACTION_DOWN, 0);
        compareKey(sentEvents[1], 0x800D, KEY_ACTION_DOWN, MODIFIER_EXTENDED | MODIFIER_SHIFT);
        compareKey(sentEvents[2], 0x800D, KEY_ACTION_UP, releaseKeypad ? MODIFIER_EXTENDED : 0);
        compareKey(sentEvents[3], 0x800D, KEY_ACTION_UP, releaseKeypad ? 0 : MODIFIER_EXTENDED);
    }

    void focusLossReleasesBothEnterKeysAndPreservesFlags()
    {
        StreamingPreferences preferences;
        SdlInputHandler input(preferences, 1920, 1080);
        sendKey(input, SDL_SCANCODE_RETURN, SDL_PRESSED);
        sendKey(input, SDL_SCANCODE_KP_ENTER, SDL_PRESSED, KMOD_LSHIFT);
        sendKey(input, SDL_SCANCODE_INTERNATIONAL1, SDL_PRESSED, KMOD_LCTRL);
        QCOMPARE(sentEvents.size(), 3);
        compareKey(sentEvents[2], 0x80E2, KEY_ACTION_DOWN, MODIFIER_CTRL, SS_KBE_FLAG_NON_NORMALIZED);
        sentEvents.clear();

        // notifyFocusLost() invokes this same keyboard release path.
        input.raiseAllKeys();
        QCOMPARE(sentEvents.size(), 3);
        std::sort(sentEvents.begin(), sentEvents.end(), [](const auto& a, const auto& b) {
            return std::tie(a.code, a.modifiers) < std::tie(b.code, b.modifiers);
        });
        compareKey(sentEvents[0], 0x800D, KEY_ACTION_UP, 0);
        compareKey(sentEvents[1], 0x800D, KEY_ACTION_UP, MODIFIER_EXTENDED);
        compareKey(sentEvents[2], 0x80E2, KEY_ACTION_UP, 0, SS_KBE_FLAG_NON_NORMALIZED);

        sentEvents.clear();
        input.raiseAllKeys();
        QVERIFY(sentEvents.isEmpty());
    }
};

QTEST_APPLESS_MAIN(KeyboardTest)
#include "tst_keyboard.moc"
