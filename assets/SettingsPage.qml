// SettingsPage: the account (name, number, username), notifications per kind of chat, devices signed in to the
// account, archive, sign out, about.
import bb.cascades 1.3
import bb.system 1.2
import gram 1.0

Page {
    id: page
    signal goBack()
    signal openArchive()
    signal signOut()
    signal editProfile()
    signal openLock()
    signal openAppearance()
    actionBarVisibility: ChromeVisibility.Hidden
    property variant me: store.me
    property variant scopes: store.notifyScopes
    property variant priv: store.privacy
    property string privKey: ""
    onCreationCompleted: { store.loadSessions(); store.loadPrivacy(); }
    function askPrivacy(key, title) {
        page.privKey = key;
        privDialog.title = title;
        privDialog.clearList();
        var now = page.priv[key] ? page.priv[key] : "", names = ["Everybody", "My contacts", "Nobody"];
        for (var i = 0; i < 3; i++) privDialog.appendItem(names[i], true, now.indexOf(names[i]) === 0);
        privDialog.show();
    }
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: "Settings"; onBackClicked: page.goBack() }
        ScrollView {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                bottomPadding: ui.du(4)
                Container {                      // account card (tap: edit name, bio, username)
                    gestureHandlers: [ TapHandler { onTapped: page.editProfile() } ]
                    horizontalAlignment: HorizontalAlignment.Fill
                    background: Color.create(Skin.headerBg)
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    leftPadding: ui.du(3); topPadding: ui.du(2.5); bottomPadding: ui.du(3)
                    Avatar { photo: page.me.photo ? page.me.photo : ""; initials: page.me.initials ? page.me.initials : ""; tint: page.me.color ? page.me.color : "#65aadd"; mask: "header"; size: ui.du(15) }
                    Container {
                        leftMargin: ui.du(3)
                        verticalAlignment: VerticalAlignment.Center
                        Label { text: page.me.name ? page.me.name : ""; textStyle { base: SystemDefaults.TextStyles.TitleText; color: Color.White; fontWeight: FontWeight.W500 } }
                        Label { text: page.me.phone ? page.me.phone : ""; topMargin: 0; textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.headerDim) } }
                        Label { visible: page.me.username ? true : false; text: page.me.username ? page.me.username : ""; topMargin: 0; textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.headerText) } }
                    }
                }
                Section { text: "Notifications" }
                OptionRow { icon: "bell"; title: "Private chats"; hasSwitch: true; checked: page.scopes.private === true; onToggled: store.setScopeOn("private", on) }
                OptionRow { icon: "group"; title: "Groups"; hasSwitch: true; checked: page.scopes.groups === true; onToggled: store.setScopeOn("groups", on) }
                OptionRow { icon: "megaphone"; title: "Channels"; hasSwitch: true; checked: page.scopes.channels === true; onToggled: store.setScopeOn("channels", on) }
                Label {
                    multiline: true
                    leftMargin: ui.du(3); rightMargin: ui.du(3)
                    text: "Notifications keep coming when Gram is closed: a small background part of Gram stays connected."
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) }
                }
                Section { text: "Privacy" }
                OptionRow { icon: "lock"; title: "App lock"; sub: appLock.enabled ? "On · " + appLock.afterName(appLock.after).toLowerCase() : "Off"; onClicked: page.openLock() }
                OptionRow { icon: "lock"; title: "Phone number"; sub: page.priv.phone ? page.priv.phone : "…"; onClicked: page.askPrivacy("phone", "Who can see my phone number") }
                OptionRow { icon: "lock"; title: "Last seen and online"; sub: page.priv.lastSeen ? page.priv.lastSeen : "…"; onClicked: page.askPrivacy("lastSeen", "Who can see when I was online") }
                OptionRow { icon: "lock"; title: "Profile photo"; sub: page.priv.photo ? page.priv.photo : "…"; onClicked: page.askPrivacy("photo", "Who can see my profile photo") }
                OptionRow { icon: "lock"; title: "Forwarded messages"; sub: page.priv.forwards ? page.priv.forwards : "…"; onClicked: page.askPrivacy("forwards", "Who can link to my account when forwarding my messages") }
                OptionRow { icon: "lock"; title: "Calls"; sub: page.priv.calls ? page.priv.calls : "…"; onClicked: page.askPrivacy("calls", "Who can call me") }
                OptionRow { icon: "lock"; title: "Groups and channels"; sub: page.priv.invites ? page.priv.invites : "…"; onClicked: page.askPrivacy("invites", "Who can add me to groups and channels") }
                Section { text: "Chats" }
                OptionRow { icon: "settings"; title: "Appearance"; sub: (skin.mode === "light" ? "Light" : "Dark") + " · " + skin.accentName; onClicked: page.openAppearance() }
                OptionRow { icon: "archive"; title: "Archived chats"; onClicked: page.openArchive() }
                Section { text: "Devices" }
                Container {
                    id: devices
                    horizontalAlignment: HorizontalAlignment.Fill
                    property variant rows: store.sessions
                    onRowsChanged: devices.fill()
                    onCreationCompleted: devices.fill()
                    function fill() {
                        devices.removeAll();
                        for (var i = 0; i < rows.length; i++) {
                            var r = sessionDef.createObject();
                            r.title = rows[i].title + (rows[i].current ? " (this phone)" : "");
                            r.sub = rows[i].device + "\n" + rows[i].place;
                            r.icon = rows[i].current ? "done" : "close";
                            r.sessionId = rows[i].id;
                            r.current = rows[i].current;
                            r.ask.connect(page.askEnd);
                            devices.add(r);
                        }
                    }
                }
                Section { text: "Account" }
                OptionRow { icon: "logout"; title: "Sign out"; danger: true; onClicked: page.signOut() }
                Label {
                    multiline: true
                    topMargin: ui.du(4)
                    horizontalAlignment: HorizontalAlignment.Center
                    text: "Gram " + appVersion + " for BlackBerry 10\nbuilt on TDLib 1.8.70"
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim); textAlign: TextAlign.Center }
                }
            }
        }
    }
    function askEnd(id, title) { endDlg.sessionId = id; endDlg.body = "Sign out " + title + "?"; endDlg.show(); }
    attachedObjects: [
        SystemListDialog {
            id: privDialog
            selectionMode: ListSelectionMode.Single
            confirmButton.label: "Save"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection && selectedIndices.length) store.setPrivacy(page.privKey, selectedIndices[0])
        },
        ComponentDefinition {
            id: sessionDef
            OptionRow {
                property string sessionId: ""
                property bool current: false
                signal ask(string id, string title)
                onClicked: if (!current) ask(sessionId, title)
            }
        },
        SystemDialog {
            id: endDlg
            property string sessionId: ""
            title: "End session"
            confirmButton.label: "Sign it out"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection) store.endSession(endDlg.sessionId)
        }
    ]
}
