// PinPad: title, four dots, a message line and a 3x4 number pad (1-9, blank, 0, backspace). Digits from the
// keyboard work too (the Passport's digit keys, or W E R / S D F / Z X C as on BlackBerry phone fields).
// Emits entered(pin) after the 4th digit; the owner calls fail(text) to shake and clear, or reset().
import bb.cascades 1.3
import gram 1.0

Container {
    id: pad
    property string title: "Enter your PIN"
    property string note: ""
    property string override: ""          // shown instead of note (the wait countdown)
    property bool noteError: false
    property bool active: true
    property string pin: ""
    signal entered(string pin)
    horizontalAlignment: HorizontalAlignment.Fill

    function press(d) {
        if (!pad.active || pad.pin.length >= 4) return;
        pad.pin = pad.pin + d;
        if (pad.pin.length === 4) done.start();
    }
    function back() { if (pad.pin.length > 0) pad.pin = pad.pin.substring(0, pad.pin.length - 1); }
    function reset() { pad.pin = ""; }
    function fail(text) { pad.note = text; pad.noteError = true; shake.play(); pad.pin = ""; }
    function key(e) {
        var c = e.unicode, map = { w: "1", e: "2", r: "3", s: "4", d: "5", f: "6", z: "7", x: "8", c: "9" };
        if (c >= "0" && c <= "9") pad.press(c);
        else if (map[c.toLowerCase()] !== undefined) pad.press(map[c.toLowerCase()]);
        else if (e.key === 8 || c === "\b") pad.back();
    }

    Label {
        horizontalAlignment: HorizontalAlignment.Center
        text: pad.title
        textStyle { base: SystemDefaults.TextStyles.TitleText; color: Color.create(Skin.text) }
    }
    Container {
        id: dots
        topMargin: ui.du(3)
        horizontalAlignment: HorizontalAlignment.Center
        layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
        animations: [
            SequentialAnimation {
                id: shake
                TranslateTransition { toX: -30; duration: 50 }
                TranslateTransition { toX: 30; duration: 80 }
                TranslateTransition { toX: -20; duration: 70 }
                TranslateTransition { toX: 12; duration: 60 }
                TranslateTransition { toX: 0; duration: 50 }
            }
        ]
    }
    Label {
        horizontalAlignment: HorizontalAlignment.Center
        topMargin: ui.du(2)
        text: pad.override !== "" ? pad.override : pad.note === "" ? " " : pad.note
        multiline: true
        textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(pad.noteError || pad.override !== "" ? Skin.error : Skin.dim); textAlign: TextAlign.Center }
    }
    Container {
        id: keys
        topMargin: ui.du(3)
        horizontalAlignment: HorizontalAlignment.Center
    }
    onCreationCompleted: {
        for (var i = 0; i < 4; i++) { var d = dotDef.createObject(); d.index = i; dots.add(d); }
        var rows = [["1", "2", "3"], ["4", "5", "6"], ["7", "8", "9"], ["", "0", "<"]];
        for (var r = 0; r < 4; r++) {
            var row = rowDef.createObject();
            for (var k = 0; k < 3; k++) { var b = keyDef.createObject(); b.digit = rows[r][k]; b.pressed.connect(pad.onKey); row.add(b); }
            keys.add(row);
        }
    }
    function onKey(d) { if (d === "<") pad.back(); else if (d !== "") pad.press(d); }
    attachedObjects: [
        ComponentDefinition {
            id: dotDef
            Container {
                id: dotC
                property int index: 0
                leftMargin: ui.du(2); rightMargin: ui.du(2)
                ImageView {
                    imageSource: "asset:///icons/disc.png"
                    filterColor: Color.create(pad.pin.length > dotC.index ? Skin.accent : Skin.badgeMuted)
                    preferredWidth: ui.du(3); preferredHeight: ui.du(3)
                }
            }
        },
        ComponentDefinition {
            id: rowDef
            Container { layout: StackLayout { orientation: LayoutOrientation.LeftToRight } }
        },
        ComponentDefinition {
            id: keyDef
            Container {
                id: kb
                property string digit: ""
                property bool down: false
                signal pressed(string d)
                layout: DockLayout {}
                preferredWidth: ui.du(19); preferredHeight: ui.du(13); leftMargin: ui.du(1); rightMargin: ui.du(1); topMargin: ui.du(0.6)
                onTouch: { if (event.isDown()) kb.down = true; else if (event.isUp() || event.isCancel()) kb.down = false; }
                gestureHandlers: [ TapHandler { onTapped: kb.pressed(kb.digit) } ]
                ImageView {
                    visible: kb.digit !== "" && kb.digit !== "<"
                    imageSource: "asset:///icons/disc.png"
                    filterColor: Color.create(kb.down ? Skin.accent : Skin.panelBg)
                    preferredWidth: ui.du(12); preferredHeight: ui.du(12)
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                }
                Label {
                    visible: kb.digit !== "<"
                    text: kb.digit
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                    textStyle { base: SystemDefaults.TextStyles.BigText; color: Color.create(Skin.text) }
                }
                ImageView {
                    visible: kb.digit === "<"
                    imageSource: "asset:///icons/backspace.png"
                    filterColor: Color.create(kb.down ? Skin.accent : Skin.dim)
                    preferredWidth: ui.du(6); preferredHeight: ui.du(6)
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                }
            }
        },
        QTimer { id: done; interval: 120; singleShot: true; onTimeout: pad.entered(pad.pin) }
    ]
}
