// LockPage (Settings > App lock): on/off, when to lock, hiding message text in notifications, change PIN.
// Setting a PIN: enter it twice in a sheet.
import bb.cascades 1.3
import bb.system 1.2
import gram 1.0

Page {
    id: page
    signal goBack()
    actionBarVisibility: ChromeVisibility.Hidden
    property string first: ""                 // the PIN typed the first time, while confirming
    property bool changing: false
    function startSetup(change) {
        page.changing = change; page.first = "";
        setup.reset(); setup.title = change ? "Enter a new PIN" : "Choose a 4-digit PIN"; setup.note = ""; setup.noteError = false;
        setupSheet.open();
    }
    function onSetupPin(pin) {
        if (page.first === "") { page.first = pin; setup.reset(); setup.title = "Enter the PIN again"; setup.note = ""; setup.noteError = false; return; }
        if (pin !== page.first) { page.first = ""; setup.title = page.changing ? "Enter a new PIN" : "Choose a 4-digit PIN"; setup.fail("The PINs didn't match. Try again."); return; }
        appLock.setPin(pin);
        setupSheet.close();
        toast.body = page.changing ? "PIN changed" : "App lock is on";
        toast.show();
    }
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: "App lock"; onBackClicked: page.goBack() }
        ScrollView {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                bottomPadding: ui.du(4)
                Container {
                    horizontalAlignment: HorizontalAlignment.Fill
                    topPadding: ui.du(4); bottomPadding: ui.du(2)
                    ImageView { horizontalAlignment: HorizontalAlignment.Center; imageSource: "asset:///icons/lock.png"; filterColor: Color.create(Skin.accent)
                                preferredWidth: ui.du(14); preferredHeight: ui.du(14) }
                    Label {
                        horizontalAlignment: HorizontalAlignment.Center
                        multiline: true
                        leftMargin: ui.du(4); rightMargin: ui.du(4)
                        text: "Ask for a PIN when Gram opens, so others using this phone can't read your chats."
                        textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.dim); textAlign: TextAlign.Center }
                    }
                }
                OptionRow {
                    id: lockRow
                    icon: "lock"; title: "App lock"; hasSwitch: true; checked: appLock.enabled
                    sub: appLock.enabled ? "On" : "Off"
                    onToggled: { if (on) page.startSetup(false); else offDlg.show(); }
                }
                Container {
                    visible: appLock.enabled
                    horizontalAlignment: HorizontalAlignment.Fill
                    Section { text: "Options" }
                    OptionRow { icon: "settings"; title: "Lock"; sub: appLock.afterName(appLock.after); onClicked: page.askAfter() }
                    OptionRow { icon: "bell"; title: "Hide message text in notifications"; sub: "Notifications say only \"Gram\" and how many messages arrived"
                                hasSwitch: true; checked: appLock.hideText; onToggled: appLock.setHideText(on) }
                    OptionRow { icon: "edit"; title: "Change PIN"; onClicked: page.startSetup(true) }
                    Label {
                        multiline: true
                        topMargin: ui.du(2)
                        leftMargin: ui.du(3); rightMargin: ui.du(3)
                        text: "If you forget the PIN, the only way back in is to sign out and sign in again. While Gram is locked, its home-screen tile shows no chats."
                        textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) }
                    }
                }
            }
        }
    }
    property variant afterValues: [0, 60, 300, 3600]
    function askAfter() {
        afterDlg.clearList();
        for (var i = 0; i < page.afterValues.length; i++) afterDlg.appendItem(appLock.afterName(page.afterValues[i]), true, appLock.after === page.afterValues[i]);
        afterDlg.show();
    }
    attachedObjects: [
        SystemToast { id: toast },
        SystemListDialog {
            id: afterDlg
            title: "Lock Gram"
            body: "How long Gram may be in the background (minimized or behind another app) before it asks for the PIN again."
            selectionMode: ListSelectionMode.Single
            confirmButton.label: "Save"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection && selectedIndices.length) appLock.setAfter(page.afterValues[selectedIndices[0]])
        },
        SystemDialog {
            id: offDlg
            title: "Turn off app lock?"
            body: "Anyone using this phone will be able to open Gram."
            confirmButton.label: "Turn off"
            cancelButton.label: "Cancel"
            onFinished: { if (value === SystemUiResult.ConfirmButtonSelection) appLock.disable(); else lockRow.sync(); }
        },
        Sheet {
            id: setupSheet
            peekEnabled: false
            onClosed: lockRow.sync()      // cancelled: the switch goes back to off
            content: Page {
                actionBarVisibility: ChromeVisibility.Hidden
                keyListeners: [ KeyListener { onKeyPressed: setup.key(event) } ]
                Container {
                    background: Color.create(Skin.chatBg)
                    TopBar { title: page.changing ? "Change PIN" : "Set a PIN"; firstIcon: "close"; onFirstClicked: setupSheet.close() }
                    Container {
                        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                        layout: DockLayout {}
                        PinPad {
                            id: setup
                            horizontalAlignment: HorizontalAlignment.Fill
                            verticalAlignment: VerticalAlignment.Center
                            onEntered: page.onSetupPin(pin)
                        }
                    }
                }
            }
        }
    ]
}
