// LockSheet: the lock screen over the whole app while appLock.locked. PIN pad, wait countdown after five wrong
// tries, and "Forgot PIN?" (the only way out: sign out, which also removes the lock).
// If a face is enrolled (in Glance, shared store), a "Unlock with face" option shows the front-camera gate;
// a verified live match unlocks without the PIN. The PIN is always the fallback.
import bb.cascades 1.3
import bb.system 1.2
import gram 1.0

Sheet {
    id: sheet
    peekEnabled: false
    signal forgot()
    property bool faceMode: false
    function refresh() { sheet.faceMode = false; gate.stop(); pin.reset(); pin.note = ""; pin.noteError = false; }
    function toFace() { sheet.faceMode = true; gate.verify(); }
    function toPin()  { gate.stop(); sheet.faceMode = false; pin.reset(); }
    content: Page {
        actionBarVisibility: ChromeVisibility.Hidden
        keyListeners: [ KeyListener { onKeyPressed: if (!sheet.faceMode) pin.key(event) } ]
        Container {
            background: Color.create(Skin.chatBg)
            layout: DockLayout {}

            // ---- PIN view ----
            Container {
                visible: !sheet.faceMode
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Center
                ImageView {
                    horizontalAlignment: HorizontalAlignment.Center
                    imageSource: "asset:///appicon.png"
                    preferredWidth: ui.du(14); preferredHeight: ui.du(14)
                }
                PinPad {
                    id: pin
                    topMargin: ui.du(3)
                    title: "Enter your PIN"
                    active: appLock.wait === 0
                    override: appLock.wait > 0 ? "Too many tries. Try again in " + appLock.wait + " s" : ""
                    onEntered: { if (appLock.unlock(pin)) sheet.close(); }
                }
                Label {
                    visible: faceLock.enrolled && appLock.wait === 0
                    horizontalAlignment: HorizontalAlignment.Center
                    topMargin: ui.du(3)
                    text: "◉  Unlock with face"
                    textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.link) }
                    gestureHandlers: [ TapHandler { onTapped: sheet.toFace() } ]
                }
                Label {
                    horizontalAlignment: HorizontalAlignment.Center
                    topMargin: ui.du(3)
                    text: "Forgot PIN?"
                    textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.link) }
                    gestureHandlers: [ TapHandler { onTapped: forgotDlg.show() } ]
                }
            }

            // ---- Face view ----
            Container {
                visible: sheet.faceMode
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
                layout: DockLayout {}
                FaceGate {
                    id: gate
                    lock: faceLock
                    previewSize: 680
                    onUnlocked: { appLock.unlockByFace(); sheet.close(); }
                    onFailed:   { sheet.toPin(); toast.body = "Face not recognised — enter your PIN"; toast.show(); }
                }
                Label {
                    horizontalAlignment: HorizontalAlignment.Center
                    verticalAlignment: VerticalAlignment.Bottom
                    bottomMargin: ui.du(2)
                    text: "Use PIN instead"
                    textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.White }
                    gestureHandlers: [ TapHandler { onTapped: sheet.toPin() } ]
                }
            }
        }
    }
    onCreationCompleted: appLock.wrongPin.connect(sheet.wrong)
    function wrong(left) { pin.fail(left > 0 ? "Wrong PIN" + (left <= 2 ? " (" + left + (left === 1 ? " try" : " tries") + " left)" : "") : ""); }
    attachedObjects: [
        SystemToast { id: toast },
        SystemDialog {
            id: forgotDlg
            title: "Forgot your PIN?"
            body: "The PIN can't be recovered. The only way back in is to sign out of Telegram on this phone and sign in again with your phone number. Your chats stay in your Telegram account; downloaded files on this phone are removed."
            confirmButton.label: "Sign out"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection) sheet.forgot()
        }
    ]
}
