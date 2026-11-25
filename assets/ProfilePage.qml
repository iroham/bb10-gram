// ProfilePage: info about a chat: photo, name, status, bio or description, username, phone, notifications,
// members of a group.
import bb.cascades 1.3
import bb.system 1.2
import gram 1.0

Page {
    id: page
    property variant p: store.profile
    property bool fromChat: true          // opened from that chat (no "Send message" then)
    signal goBack()
    signal openChat(variant id)
    signal showPhoto(string path)
    signal addMembers(variant chatId)
    signal openAdmin(string what, variant userId)
    actionBarVisibility: ChromeVisibility.Hidden
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: page.p.isUser ? "Info" : page.p.kind === "channel" ? "Channel info" : "Group info"; onBackClicked: page.goBack() }
        ScrollView {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                bottomPadding: ui.du(4)
                Container {
                    horizontalAlignment: HorizontalAlignment.Fill
                    background: Color.create(Skin.headerBg)
                    topPadding: ui.du(3); bottomPadding: ui.du(3)
                    Avatar {
                        horizontalAlignment: HorizontalAlignment.Center
                        photo: page.p.photo ? page.p.photo : ""; initials: page.p.initials ? page.p.initials : ""; tint: page.p.color ? page.p.color : "#65aadd"
                        mask: "header"; size: ui.du(26)
                        gestureHandlers: [ TapHandler { onTapped: if (page.p.photo) page.showPhoto(page.p.photo) } ]
                    }
                    Label { horizontalAlignment: HorizontalAlignment.Center; text: page.p.title ? page.p.title : ""; topMargin: ui.du(2)
                            textStyle { base: SystemDefaults.TextStyles.TitleText; color: Color.White; fontWeight: FontWeight.W500 } }
                    Label { horizontalAlignment: HorizontalAlignment.Center; text: page.p.status ? page.p.status : ""; topMargin: 0
                            textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(page.p.status === "online" ? (Skin.dark ? Skin.accent : Skin.headerText) : Skin.headerDim) } }
                }
                OptionRow { visible: page.p.about ? true : false; icon: "info"; title: page.p.about ? page.p.about : ""; sub: page.p.isUser ? "Bio" : "Description" }
                OptionRow { visible: page.p.username ? true : false; icon: "group"; title: page.p.username ? page.p.username : ""; sub: "Username"
                            onClicked: { store.copyTextRaw("https://t.me/" + page.p.username.substring(1)); } }
                OptionRow { visible: page.p.phone ? true : false; icon: "call"; title: page.p.phone ? page.p.phone : ""; sub: "Phone" }
                OptionRow { visible: page.p.kind !== "saved"; icon: page.p.muted ? "muted" : "bell"; title: "Notifications"; hasSwitch: true; checked: page.p.muted === false
                            onToggled: store.setMuted(page.p.id, !on) }
                OptionRow { visible: !page.fromChat; icon: "send"; title: "Send message"; onClicked: page.openChat(page.p.id) }
                OptionRow { visible: page.p.canCall === true; icon: "call"; title: "Voice call"; sub: "End-to-end encrypted"
                            onClicked: store.calls.start(page.p.userId) }
                OptionRow { visible: page.p.canSecret === true; icon: "lock"; title: "Start secret chat"; sub: "End-to-end encrypted, only on this phone"
                            onClicked: store.startSecretChat(page.p.userId) }
                OptionRow { visible: page.p.canAdd === true; icon: "group"; title: "Add members"; onClicked: page.addMembers(page.p.id) }
                OptionRow { visible: page.p.canLink === true; icon: "copy"; title: "Invite link"; sub: page.p.inviteLink ? page.p.inviteLink : "Create and copy a link"
                            onClicked: store.copyInviteLink(page.p.id) }
                OptionRow { visible: page.p.kind === "group" || page.p.kind === "channel"; icon: "logout"; danger: true
                            title: page.p.kind === "channel" ? "Leave channel" : "Leave group"; onClicked: leave.show() }
                Container {                                  // admin tools
                    visible: page.p.isAdmin === true || page.p.canEditInfo === true
                    horizontalAlignment: HorizontalAlignment.Fill
                    Section { text: "Manage" }
                    OptionRow { visible: page.p.canEditInfo === true; icon: "edit"; title: page.p.isChannel ? "Edit channel" : "Edit group"; sub: "Photo, name and description"
                                onClicked: page.openAdmin("edit", 0) }
                    OptionRow { visible: page.p.canRestrict === true && page.p.isChannel !== true; icon: "lock"; title: "Permissions"
                                sub: "What members can do" + (page.p.slowMode ? " · slow mode on" : ""); onClicked: page.openAdmin("perms", 0) }
                    OptionRow { visible: page.p.canRestrict === true && page.p.isSuper === true; icon: "close"; title: "Removed users"; onClicked: page.openAdmin("banned", 0) }
                    OptionRow { visible: page.p.canLog === true; icon: "info"; title: "Recent actions"; sub: "What admins did in the last 48 hours"; onClicked: page.openAdmin("log", 0) }
                }
                Section { visible: page.p.members && page.p.members.length > 0; text: "Members" }
                Container {
                    id: memberBox
                    horizontalAlignment: HorizontalAlignment.Fill
                    property variant rows: page.p.members
                    onRowsChanged: memberBox.fill()
                    function fill() {
                        memberBox.removeAll();
                        if (!rows) return;
                        for (var i = 0; i < rows.length; i++) {
                            var m = memberDef.createObject();
                            m.userId = rows[i].id; m.name = rows[i].title; m.status = rows[i].preview; m.role = rows[i].date;
                            m.photo = rows[i].photo; m.initials = rows[i].initials; m.tint = rows[i].color;
                            memberBox.add(m);
                        }
                    }
                }
            }
        }
    }
    attachedObjects: [
        ComponentDefinition {
            id: memberDef
            Container {
                id: mr
                property variant userId: 0
                property string name: ""
                property string status: ""
                property string role: ""
                property alias photo: av.photo
                property alias initials: av.initials
                property alias tint: av.tint
                property bool down: false
                horizontalAlignment: HorizontalAlignment.Fill
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                background: mr.down ? Color.create(Skin.press) : Color.Transparent
                leftPadding: ui.du(3); rightPadding: ui.du(2.5); topPadding: ui.du(1); bottomPadding: ui.du(1)
                onTouch: { if (event.isDown()) mr.down = true; else if (event.isUp() || event.isCancel()) mr.down = false; }
                gestureHandlers: [ TapHandler { onTapped: store.openUserChat(mr.userId) } ]
                contextActions: [ ActionSet {
                    title: mr.name
                    ActionItem { title: "Message"; imageSource: "asset:///icons/send.png"; onTriggered: store.openUserChat(mr.userId) }
                    ActionItem { title: mr.role === "admin" ? "Edit admin rights" : "Make admin"; imageSource: "asset:///icons/settings.png"
                                 enabled: page.p.canPromote === true && mr.role !== "owner"; onTriggered: page.openAdmin("promote", mr.userId) }
                    ActionItem { title: "Restrict"; imageSource: "asset:///icons/lock.png"
                                 enabled: page.p.canRestrict === true && page.p.isSuper === true && mr.role !== "owner" && mr.role !== "admin"; onTriggered: page.openAdmin("restrict", mr.userId) }
                    DeleteActionItem { title: "Remove from group"; enabled: page.p.canRemove === true && mr.role !== "owner"
                                       onTriggered: { removeAsk.userId = mr.userId; removeAsk.title = "Remove " + mr.name + " from the group?"; removeAsk.show(); } }
                } ]
                Avatar { id: av; size: ui.du(9) }
                Container {
                    leftMargin: ui.du(2.5)
                    layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                    verticalAlignment: VerticalAlignment.Center
                    Label { text: mr.name; textStyle { base: SystemDefaults.TextStyles.PrimaryText; color: Color.create(Skin.text) } }
                    Label { text: mr.status; topMargin: 0; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(mr.status === "online" ? Skin.accent : Skin.dim) } }
                }
                Label { text: mr.role; verticalAlignment: VerticalAlignment.Center; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) } }
            }
        },
        SystemDialog {
            id: removeAsk
            property variant userId: 0
            confirmButton.label: "Remove"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection) store.removeMember(page.p.id, removeAsk.userId)
        },
        SystemDialog {
            id: leave
            title: "Leave " + (page.p.title ? page.p.title : "") + "?"
            confirmButton.label: "Leave"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection) { store.deleteChat(page.p.id); page.goBack(); }
        }
    ]
}
