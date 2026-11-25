// Gram: main window. Home page = sign-in steps or the chat lists (folder tabs); chats, info pages, settings,
// archive, photos and videos are pushed on top (swipe from the left edge or the back button returns).
import bb.cascades 1.3
import bb.system 1.2
import bb.multimedia 1.0
import gram 1.0
import "Admin.js" as Admin

NavigationPane {
    id: nav
    property bool ready: store.authState === "ready"
    property bool searching: false
    property string folder: "main"
    property int dbgStep: 0
    property variant chatPage                // the last chat page opened (tests)
    peekEnabled: true

    function say(text) { toastBox.body = text; toastBox.show(); }
    property bool callOpen: false
    function clock(s) {
        var h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, x = s % 60;
        return (h > 0 ? h + ":" + (m < 10 ? "0" : "") : "") + (m < 10 && h === 0 ? "0" : "") + m + ":" + (x < 10 ? "0" : "") + x;
    }
    function openCall() { if (store.calls.active && !nav.callOpen) { nav.callOpen = true; callSheet.open(); } }
    function callChanged() {                     // the call screen follows the call: up when one starts, away after it ended
        var c = store.calls;
        if (c.active && !nav.callSeen) { nav.callSeen = true; nav.openCall(); }
        if (!c.active) { nav.callSeen = false; if (nav.callOpen) callSheet.close(); }
        Application.mainWindow.screenIdleMode = c.active && !c.near && c.phase !== "ended" ? ScreenIdleMode.KeepAwake : ScreenIdleMode.Normal;
    }
    property bool callSeen: false
    property bool lockOpen: false
    function lockChanged() { if (appLock.locked && !nav.lockOpen) { lockSheet.refresh(); lockSheet.open(); } }
    function forgotPin() { appLock.disable(); store.logOut(); while (nav.count() > 1) nav.pop(); lockSheet.close(); }
    function openLock() { nav.push(wire(lockDef.createObject())); }
    function openAppearance() { nav.push(wire(appearanceDef.createObject())); }
    function back() { nav.pop(); }
    function wire(p) { p.goBack.connect(nav.back); return p; }
    function openChat(id) {
        if (nav.top && nav.top.isChat && nav.top.chatId === id) return;
        store.openChat(id);
        var p = wire(chatDef.createObject());
        p.chatId = id;
        nav.chatPage = p;
        p.say.connect(nav.say); p.showPhoto.connect(nav.openPhoto); p.showVideo.connect(nav.openVideo); p.showProfile.connect(nav.openProfileOfChat);
        nav.push(p);
    }
    function openPhoto(path) { var p = wire(photoDef.createObject()); p.path = path; nav.push(p); }
    function openVideo(path) { var p = wire(videoDef.createObject()); p.path = path; p.say.connect(nav.say); nav.push(p); p.start(); }
    function openProfileOfChat() { nav.openProfile(store.chat.id, true); }
    function openProfile(id, fromChat) {
        store.openProfile(id);
        var p = wire(profileDef.createObject());
        p.fromChat = fromChat;
        p.openChat.connect(nav.openChat); p.showPhoto.connect(nav.openPhoto); p.addMembers.connect(nav.openAddMembers);
        p.openAdmin.connect(nav.openAdmin);
        nav.push(p);
    }
    // admin tools: what = edit | perms | banned | log | promote | restrict (userId for the last two)
    function openAdmin(what, userId) {
        var pr = store.profile, chatId = pr.id;
        if (what === "edit") { nav.push(wire(editChatDef.createObject())); return; }
        if (what === "banned" || what === "log") {
            var l = wire(adminListDef.createObject()); l.kind = what; l.chatId = chatId; nav.push(l); l.load(); return;
        }
        var s = wire(switchDef.createObject());
        if (what === "perms") {
            s.title = "Permissions"; s.note = "What every member of the group can do.";
            s.items = Admin.items(Admin.permissions, pr.permissions, null);
            if (pr.isSuper) { s.extra = "slowmode"; s.seconds = pr.slowMode ? pr.slowMode : 0; }
            s.saved.connect(function(picked, seconds) {
                store.setPermissions(chatId, picked);
                if (pr.isSuper && seconds !== (pr.slowMode ? pr.slowMode : 0)) store.setSlowMode(chatId, seconds);
            });
        } else if (what === "promote") {
            var m = store.memberInfo(userId), isAdmin = m.role === "admin";
            s.title = isAdmin ? "Admin rights" : "New admin";
            s.note = m.name + (isAdmin ? " is an admin." : " will become an admin.") + (pr.isOwner ? "" : " You can only give rights you have.");
            s.items = Admin.items(pr.isChannel ? Admin.channelRights : Admin.groupRights, isAdmin ? m.rights : Admin.defaults(pr.isChannel), pr.isOwner ? null : pr.myRights);
            if (isAdmin) { s.danger = "Dismiss admin"; s.dangerClicked.connect(function() { store.demote(chatId, userId); }); }
            s.saved.connect(function(picked, seconds) { store.promote(chatId, userId, picked); });
        } else if (what === "restrict") {
            var r = store.memberInfo(userId);
            s.title = "Restrict"; s.note = "What " + r.name + " can do in this group.";
            s.items = Admin.items(Admin.permissions, r.permissions, null);
            s.extra = "duration";
            s.danger = "Remove from the group"; s.dangerClicked.connect(function() { store.banMember(chatId, userId); });
            s.saved.connect(function(picked, seconds) { store.restrictMember(chatId, userId, picked, seconds); });
        }
        nav.push(s);
    }
    function openArchive() {
        store.loadMoreChats("archive");
        var p = wire(archiveDef.createObject());
        p.picked.connect(nav.pickedChat);
        nav.push(p);
    }
    function openSettings() {
        var p = wire(settingsDef.createObject());
        p.openArchive.connect(nav.openArchive); p.signOut.connect(signOut.show); p.editProfile.connect(nav.openEditProfile);
        p.openLock.connect(nav.openLock); p.openAppearance.connect(nav.openAppearance);
        nav.push(p);
    }
    function pickedChat(chat) { nav.openChat(chat.id); }
    function openAddMembers(chatId) {
        var p = wire(contactsDef.createObject());
        p.mode = "add"; p.targetChat = chatId;
        nav.push(p);
    }
    function openEditProfile() { nav.push(wire(editDef.createObject())); }
    function openContacts(mode) {
        var p = wire(contactsDef.createObject());
        if (mode) p.mode = mode;
        p.openMode.connect(nav.openContacts);
        nav.push(p);
    }
    function picked(row) {
        if (row.type === "message") { nav.searching = false; nav.openChat(row.chatId); store.jumpTo(row.msgId); }
        else { nav.searching = false; nav.openChat(row.id); }
    }
    // search: local chats right away, then public chats and messages from the server, each under a heading
    property variant found: ({})
    function onFound(kind, rows) {
        if (kind === "inchat") return;
        var f = nav.found; f[kind] = rows; nav.found = f;
        foundModel.clear();
        var parts = [["chats", "Chats"], ["public", "Global search"], ["messages", "Messages"]];
        for (var i = 0; i < parts.length; i++) {
            var l = nav.found[parts[i][0]];
            if (!l || !l.length) continue;
            if (parts[i][0] !== "chats") foundModel.append({ type: "header", title: parts[i][1] });
            foundModel.append(l);
        }
    }
    function search(text) { nav.found = ({}); store.searchAll(text); }
    function toggleSearch() {
        nav.searching = !nav.searching;
        if (nav.searching) { query.text = ""; nav.search(""); query.requestFocus(); }
    }
    function debugStep() {                       // scripted check (data/debug.json), results in data/app.log
        var s = nav.dbgStep++;
        if (s === 0) {
            console.log("dbg chats " + chatsModel.size() + " folders " + JSON.stringify(store.folders));
            nav.search("a"); console.log("dbg search a -> " + foundModel.size());
            if (debug.open) { nav.openChat(debug.open); dbg.start(); }
        } else if (s === 1) {
            console.log("dbg chat " + JSON.stringify(store.chat));
            console.log("dbg messages " + messagesModel.size());
            for (var i = 0; i < messagesModel.size() && i < 12; i++) console.log("dbg row " + i + " " + JSON.stringify(messagesModel.data([i])));
            if (debug.say) store.sendText(debug.say, 0);
            nav.openProfileOfChat();
            dbg.start();
        } else if (s === 2) {
            console.log("dbg after send " + messagesModel.size() + " newest " + JSON.stringify(messagesModel.data([0])));
            console.log("dbg panel " + nav.chatPage.dbgPanel());
            console.log("dbg cover " + JSON.stringify(store.cover));
            store.loadStickers();
            console.log("dbg stickers sets " + JSON.stringify(store.stickerSets) + " shown " + store.stickerSet + " rows " + store.stickers.length + " kbd " + store.keyboard.length);
            console.log("dbg profile " + JSON.stringify(store.profile));
            nav.pop(); dbg.start();
        } else if (s === 3) {
            nav.pop(); dbg.start();
        } else if (s === 4) {
            nav.openSettings(); dbg.start();
        } else if (s === 5) {
            console.log("dbg me " + JSON.stringify(store.me) + " scopes " + JSON.stringify(store.notifyScopes) + " privacy " + JSON.stringify(store.privacy));
            nav.pop(); nav.openContacts(); dbg.start();
        } else if (s === 6) {
            nav.openContacts("group"); dbg.start();
        } else if (s === 7) {
            console.log("dbg contacts pages " + nav.count());
            nav.pop(); nav.pop();
            nav.openEditProfile(); nav.openAddMembers(-20); nav.openProfile(-20, false); dbg.start();
        } else if (s === 8) {
            console.log("dbg edit/add/profile pages " + nav.count() + " group profile " + JSON.stringify(store.profile).substring(0, 300));
            nav.pop(); nav.pop(); nav.pop();
            nav.openChat(-77); dbg.start();
        } else if (s === 9) {
            console.log("dbg secret " + JSON.stringify(store.chat).substring(0, 400));
            nav.pop();
            store.calls.demo({ phase: "ringing", outgoing: false, user: 1 });
            dbg.start();
        } else if (s === 10) {
            var c = store.calls;
            console.log("dbg call ringing: active " + c.active + " incoming " + c.incoming + " name " + c.name + " status " + c.status + " sheet " + nav.callOpen);
            c.demo({ phase: "talking", outgoing: true, user: 1, seconds: 75, emojis: "🐶 🍉 🚗 🎸" });
            dbg.start();
        } else if (s === 11) {
            var c2 = store.calls;
            console.log("dbg call talking: incoming " + c2.incoming + " seconds " + c2.seconds + " emojis " + c2.emojis + " bars " + c2.bars + " sheet " + nav.callOpen);
            callSheet.close();
            dbg.start();
        } else if (s === 12) {
            console.log("dbg call minimized: sheet " + nav.callOpen + " cover " + store.calls.active);
            nav.openCall(); store.calls.hangUp();
            console.log("dbg call hung up: phase " + store.calls.phase + " status " + store.calls.status);
            dbg.start();
        } else if (s === 13) {
            console.log("dbg call after end: active " + store.calls.active + " sheet " + nav.callOpen);
            console.log("dbg done, pages " + nav.count());
        }
    }

    onPopTransitionEnded: {
        if (page.leaving) page.leaving();
        if (page.isChat && store.chat.id === page.chatId) store.closeChat();
        page.destroy();
    }
    onCreationCompleted: {
        Application.setCover(tile);
        store.toast.connect(nav.say);
        appLock.changed.connect(nav.lockChanged);
        lockStart.start();
        store.calls.changed.connect(nav.callChanged);
        store.calls.nearChanged.connect(nav.callChanged);
        store.searchDone.connect(nav.onFound);
        store.openChatRequested.connect(nav.openChat);
        Application.thumbnail.connect(function () { store.active = false; });
        Application.invisible.connect(function () { store.active = false; });
        Application.fullscreen.connect(function () { store.active = true; });
        if (debug.qmlcheck) console.log("qmlcheck: skin " + skin.mode + " listBg " + Skin.listBg + " accent " + Skin.accent + " dark " + Skin.dark);
        if (debug.demo) dbg.start();
        if (debug.probeImage) probeImg.imageSource = debug.probeImage;
        if (debug.probeAudio) { probeAudio.sourceUrl = debug.probeAudio; console.log("probe audio prepare " + probeAudio.prepare() + " play " + probeAudio.play()); }
    }

    Menu.definition: MenuDefinition {
        settingsAction: SettingsActionItem { enabled: nav.ready; onTriggered: nav.openSettings() }
        actions: [
            ActionItem { title: "New message"; imageSource: "asset:///icons/edit.png"; enabled: nav.ready; onTriggered: nav.openContacts() },
            ActionItem { title: "Archive"; imageSource: "asset:///icons/archive.png"; enabled: nav.ready; onTriggered: nav.openArchive() }
        ]
    }

    Page {
        id: home
        actionBarVisibility: ChromeVisibility.Hidden
        Container {
            background: Color.create(Skin.listBg)
            CallBar {}
            TopBar {
                title: nav.searching ? "Search" : store.connection !== "" && nav.ready ? store.connection : "Gram"
                firstIcon: nav.ready ? (nav.searching ? "close" : "search") : ""
                secondIcon: nav.ready && !nav.searching ? "settings" : ""
                onFirstClicked: nav.toggleSearch()
                onSecondClicked: nav.openSettings()
            }
            Container {
                visible: nav.ready && nav.searching
                leftPadding: ui.du(2); rightPadding: ui.du(2); topPadding: ui.du(1.5); bottomPadding: ui.du(1)
                TextField { id: query; hintText: "Search chats, people and messages"; onTextChanging: nav.search(text) }
            }
            ScrollView {                             // folder tabs
                visible: nav.ready && !nav.searching && store.folders.length > 1
                horizontalAlignment: HorizontalAlignment.Fill
                scrollViewProperties.scrollMode: ScrollMode.Horizontal
                Container {
                    id: tabs
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    background: Color.create(Skin.headerBg)
                    leftPadding: ui.du(1)
                    property variant list: store.folders
                    onListChanged: tabs.fill()
                    function fill() {
                        tabs.removeAll();
                        for (var i = 0; i < list.length; i++) {
                            var t = tabDef.createObject();
                            t.key = list[i].key; t.text = list[i].title;
                            tabs.add(t);
                        }
                    }
                }
            }
            StoryBar {
                visible: nav.ready && !nav.searching && nav.folder === "main" && store.stories.length > 0
                onOpened: stories.start(chatId)
            }
            SignIn { visible: !nav.ready; layoutProperties: StackLayoutProperties { spaceQuota: 1 } }
            Container {
                visible: nav.ready && !nav.searching
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                layout: DockLayout {}
                ChatList {
                    horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                    listKey: nav.folder
                    dataModel: nav.folder === "main" ? chatsModel : store.list(nav.folder)
                    onPicked: nav.openChat(chat.id)
                }
                Container {
                    horizontalAlignment: HorizontalAlignment.Right; verticalAlignment: VerticalAlignment.Bottom
                    rightPadding: ui.du(3); bottomPadding: ui.du(3)
                    layout: DockLayout {}
                    ImageView { imageSource: "asset:///icons/disc.png"; filterColor: Color.create(Skin.badge); preferredWidth: ui.du(14); preferredHeight: ui.du(14) }
                    IconButton { tint: "#ffffffff"; icon: "asset:///icons/edit.png"; horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center; onClicked: nav.openContacts() }
                }
            }
            ChatList {
                visible: nav.ready && nav.searching
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                endless: false
                dataModel: ArrayDataModel { id: foundModel }
                onPicked: nav.picked(chat)
            }
        }
    }

    attachedObjects: [
        SceneCover { id: tile; content: CoverView {} },
        ComponentDefinition { id: chatDef; source: "ChatPage.qml" },
        ComponentDefinition { id: photoDef; source: "PhotoPage.qml" },
        ComponentDefinition { id: videoDef; source: "VideoPage.qml" },
        ComponentDefinition { id: archiveDef; source: "ArchivePage.qml" },
        ComponentDefinition { id: profileDef; source: "ProfilePage.qml" },
        ComponentDefinition { id: editChatDef; source: "EditChatPage.qml" },
        ComponentDefinition { id: adminListDef; source: "AdminListPage.qml" },
        ComponentDefinition { id: switchDef; source: "SwitchListPage.qml" },
        ComponentDefinition { id: settingsDef; source: "SettingsPage.qml" },
        ComponentDefinition { id: contactsDef; source: "ContactsPage.qml" },
        ComponentDefinition { id: editDef; source: "EditProfilePage.qml" },
        ComponentDefinition { id: lockDef; source: "LockPage.qml" },
        ComponentDefinition { id: appearanceDef; source: "AppearancePage.qml" },
        StoryViewer { id: stories },
        LockSheet { id: lockSheet; onOpened: nav.lockOpen = true; onClosed: nav.lockOpen = false; onForgot: nav.forgotPin() },
        QTimer { id: lockStart; interval: 50; singleShot: true; onTimeout: nav.lockChanged() },
        ComponentDefinition {
            id: tabDef
            Container {
                id: tab
                property string key: ""
                property alias text: tabLabel.text
                property bool on: nav.folder === tab.key
                topPadding: ui.du(1.6); bottomPadding: ui.du(1.2); leftPadding: ui.du(2.2); rightPadding: ui.du(2.2)
                gestureHandlers: [ TapHandler { onTapped: nav.folder = tab.key } ]
                Label { id: tabLabel; textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(tab.on ? (Skin.dark ? Skin.accent : Skin.headerText) : Skin.headerDim); fontWeight: tab.on ? FontWeight.W500 : FontWeight.Normal } }
                Container { background: Color.create(tab.on ? (Skin.dark ? Skin.accent : Skin.headerText) : "#00000000"); preferredHeight: ui.du(0.5); minHeight: ui.du(0.5); horizontalAlignment: HorizontalAlignment.Fill; topMargin: ui.du(0.8) }
            }
        },
        SystemToast { id: toastBox },
        CallSheet { id: callSheet; onOpened: nav.callOpen = true; onClosed: nav.callOpen = false },
        SystemDialog {
            id: signOut
            title: "Sign out?"
            body: "This removes the account and its downloaded messages from this app. You can sign in again later."
            confirmButton.label: "Sign out"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection) { store.logOut(); while (nav.count() > 1) nav.pop(); }
        },
        ImageTracker { id: probeImg; onStateChanged: console.log("probe image state " + state + " size " + width + "x" + height) },
        MediaPlayer { id: probeAudio; onMediaStateChanged: console.log("probe audio state " + mediaState); onError: console.log("probe audio error " + mediaError); onDurationChanged: console.log("probe audio duration " + duration) },
        QTimer { id: dbg; interval: 3000; singleShot: true; onTimeout: nav.debugStep() }
    ]
}
