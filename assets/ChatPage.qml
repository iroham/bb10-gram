// ChatPage: the open chat: header (tap = info), pinned message, messages (newest at the bottom), jump-to-latest,
// search in the chat, reply/edit bar, input with attachments. Draft is kept when leaving.
import bb.cascades 1.3
import bb.system 1.2
import bb.multimedia 1.0
import bb.cascades.pickers 1.0
import gram 1.0
import "Emoji.js" as E

Page {
    id: page
    property bool isChat: true
    property variant chatId: 0
    property variant chat: store.chat
    property variant replyTo: 0
    property variant editing: 0
    property string barText: ""
    property bool searching: false
    property bool scrolledUp: false
    property variant reactTo: 0
    property string fieldText: field.text
    property bool recording: false
    property string recUrl: ""
    property int recSecs: 0
    property double recStart: 0
    function startRecording() {
        page.panel = "";
        page.recUrl = store.recordPath();
        recorder.outputUrl = page.recUrl;
        var e = recorder.record();
        if (e !== MediaError.None) { page.say("Cannot record (" + e + "). Allow the microphone for Gram in Settings > Security and Privacy > Application Permissions."); store.discardFile(page.recUrl); return; }
        page.recStart = Date.now(); page.recSecs = 0; page.recording = true; recTick.start();
    }
    function stopRecording(sendIt) {
        if (!page.recording) return;
        page.recording = false;
        recTick.stop();
        page.recSecs = Math.round((Date.now() - page.recStart) / 1000);
        recorder.reset();
        if (sendIt && page.recSecs >= 1) { store.sendVoice(page.recUrl, page.recSecs); store.toLatest(); }
        else { store.discardFile(page.recUrl); if (sendIt) page.say("Too short: record at least a second."); }
    }
    property bool hasText: false             // the field has text (updated while typing)
    property string panel: ""                // "", "stickers", "keyboard": what shows under the input
    property variant stickerSets: store.stickerSets
    property variant stickerRows: store.stickers
    property variant keys: store.keyboard
    property variant gifRows: store.gifs
    property variant shownSet: store.stickerSet
    onStickerSetsChanged: if (page.mediaTab === "stickers") page.fillChips()
    onShownSetChanged: if (page.mediaTab === "stickers") page.fillChips()
    onStickerRowsChanged: if (page.mediaTab === "stickers") page.fillGrid()
    onGifRowsChanged: if (page.mediaTab === "gifs") page.fillGrid()
    onKeysChanged: {
        keyBox.removeAll();
        if (!keys || keys.length === 0) { if (page.panel === "keyboard") page.panel = ""; return; }
        for (var r = 0; r < keys.length; r++) {
            var line = keyLineDef.createObject(); keyBox.add(line);
            for (var c = 0; c < keys[r].length; c++) { var k = keyDef.createObject(); k.row = r; k.col = c; k.text = keys[r][c]; line.add(k); }
        }
        if (page.panel === "") page.panel = "keyboard";
    }
    property string mediaTab: "emoji"
    property int gridCount: 0                // items shown in the panel grid (a property, so bindings update)
    property int emojiCat: 0
    function togglePanel(p) {
        page.panel = page.panel === p ? "" : p;
        if (page.panel === "media") page.showTab(page.mediaTab);
    }
    function showTab(t) {
        page.mediaTab = t;
        if (t === "stickers" && (!page.stickerSets || page.stickerSets.length === 0)) store.loadStickers();
        if (t === "gifs") store.loadGifs();
        page.fillGrid(); page.fillChips();
    }
    function fillGrid() {
        gridModel.clear();
        if (page.mediaTab === "emoji") {
            var l = E.list(page.emojiCat), rows = [];
            for (var i = 0; i < l.length; i++) rows.push({ e: l[i] });
            gridModel.append(rows);
        } else if (page.mediaTab === "stickers") { if (page.stickerRows && page.stickerRows.length) gridModel.append(page.stickerRows); }
        else if (store.gifs.length) gridModel.append(store.gifs);
        page.gridCount = gridModel.size();
        grid.scrollToPosition(ScrollPosition.Beginning, ScrollAnimation.None);
    }
    function fillChips() {
        chipBox.removeAll();
        if (page.mediaTab === "emoji") {
            for (var i = 0; i < E.cats.length; i++) { var c = chipDef.createObject(); c.key = i; c.text = E.cats[i][0]; c.on = i === page.emojiCat; c.big = true; c.picked.connect(page.pickChip); chipBox.add(c); }
        } else if (page.mediaTab === "stickers" && page.stickerSets) {
            for (var k = 0; k < page.stickerSets.length; k++) { var s = chipDef.createObject(); s.key = page.stickerSets[k].id; s.text = page.stickerSets[k].title; s.on = s.key == store.stickerSet; s.picked.connect(page.pickChip); chipBox.add(s); }
        }
    }
    function pickChip(key) {
        if (page.mediaTab === "emoji") { page.emojiCat = key; page.fillGrid(); page.fillChips(); }
        else store.showStickerSet(key);
    }
    function dbgPanel() {                      // tests: open each tab and report what it shows
        var out = [];
        page.togglePanel("media"); out.push("emoji " + gridModel.size() + " chips " + chipBox.count());
        page.insertText("😀"); out.push("field " + field.text.length);
        page.backspace(); out.push("after backspace " + field.text.length);
        page.showTab("stickers"); out.push("stickers " + gridModel.size() + " chips " + chipBox.count());
        page.showTab("gifs"); out.push("gifs " + gridModel.size());
        return out.join(", ");
    }
    function insertText(t) {
        field.editor.insertPlainText(t);
        page.hasText = true;
    }
    function backspace() {                   // delete the character (or emoji) before the cursor
        var s = field.text, p = field.editor.cursorPosition;   // the cursor counts characters, not UTF-16 units
        if (p <= 0) return;
        var starts = [], i = 0;
        while (i < s.length) { starts.push(i); var c = s.charCodeAt(i); i += (c >= 0xD800 && c <= 0xDBFF && i + 1 < s.length) ? 2 : 1; }
        if (p > starts.length) p = starts.length;
        var from = p - 1;
        if (s.charCodeAt(starts[from]) === 0xFE0F && from > 0) from--;   // emoji + variation selector
        var end = p < starts.length ? starts[p] : s.length;
        field.text = s.substring(0, starts[from]) + s.substring(end);
        field.editor.cursorPosition = from;
        page.hasText = field.text !== "";
    }
    signal showPhoto(string path)
    signal showVideo(string path)
    signal showProfile()
    signal goBack()
    signal say(string text)
    actionBarVisibility: ChromeVisibility.Hidden

    function clearBar() { page.replyTo = 0; page.editing = 0; page.barText = ""; }
    function sendNow() {
        var s = field.text;
        if (s.replace(/\s/g, "") === "") return;
        if (page.editing) store.editText(page.editing, s); else store.sendText(s, page.replyTo);
        field.text = "";
        page.clearBar();
        store.toLatest();
    }
    function play(url) {
        if (player.sourceUrl == url && player.mediaState === MediaState.Started) { player.pause(); return; }
        if (player.sourceUrl != url) { player.stop(); player.sourceUrl = url; }
        player.play();
    }
    function onScrollTo(i) { list.scrollToItem([i], ScrollAnimation.None); }
    function onNewest(out) { if (out || !page.scrolledUp) toBottom.start(); }    // own message: always show it
    function toggleSearch() {
        page.searching = !page.searching;
        found.clear();
        if (page.searching) { query.text = ""; query.requestFocus(); }
    }
    function onFound(kind, rows) { if (kind === "inchat" && page.searching) { found.clear(); if (rows.length) found.append(rows); } }
    onCreationCompleted: {
        store.scrollTo.connect(page.onScrollTo);
        store.newestAdded.connect(page.onNewest);
        store.searchDone.connect(page.onFound);
        store.playFile.connect(page.play);
        for (var i = 0; i < reactions.emojis.length; i++) reactions.appendItem(reactions.emojis[i]);
        attachMenu.appendItem("Photo or video"); attachMenu.appendItem("File");
        if (page.chat.draftText) field.text = page.chat.draftText;
    }
    function leaving() {                    // called by main.qml before the page goes away
        page.stopRecording(false);
        store.scrollTo.disconnect(page.onScrollTo);
        store.newestAdded.disconnect(page.onNewest);
        store.searchDone.disconnect(page.onFound);
        store.playFile.disconnect(page.play);
        if (!page.editing) store.saveDraft(field.text);
        player.stop();
    }

    Container {
        background: Color.create(Skin.chatBg)
        CallBar {}
        TopBar {
            back: true; avatar: true
            title: page.chat.title ? page.chat.title : ""
            subtitle: page.chat.subtitle ? page.chat.subtitle : ""
            subtitleAccent: page.chat.typing === true || page.chat.subtitle === "online"
            photo: page.chat.photo ? page.chat.photo : ""
            initials: page.chat.initials ? page.chat.initials : ""
            tint: page.chat.color ? page.chat.color : "#65aadd"
            firstIcon: page.searching ? "close" : page.chat.canCall === true ? "call" : "search"
            secondIcon: !page.searching && page.chat.canCall === true ? "search" : ""
            onFirstClicked: if (!page.searching && page.chat.canCall === true) store.calls.start(page.chat.callUser); else page.toggleSearch()
            onSecondClicked: page.toggleSearch()
            onBackClicked: page.goBack()
            onTitleClicked: page.showProfile()
        }
        Container {                                // search field
            visible: page.searching
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.create(Skin.panelBg)
            leftPadding: ui.du(2); rightPadding: ui.du(2); bottomPadding: ui.du(1)
            TextField {
                id: query; hintText: "Search in this chat"
                input { submitKey: SubmitKey.Search; onSubmitted: store.searchInChat(query.text) }
            }
        }
        Container {                                // pinned message
            visible: !page.searching && page.chat.pinnedText ? true : false
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.create(Skin.panelBg)
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            leftPadding: ui.du(2); topPadding: ui.du(1); bottomPadding: ui.du(1); rightPadding: ui.du(2)
            gestureHandlers: [ TapHandler { onTapped: store.jumpTo(page.chat.pinnedId) } ]
            Container { background: Color.create(Skin.accent); minWidth: ui.du(0.6); preferredWidth: ui.du(0.6); verticalAlignment: VerticalAlignment.Fill }
            Container {
                leftMargin: ui.du(1.5)
                Label { text: "Pinned message"; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.accent); fontWeight: FontWeight.W500 } }
                Label { text: page.chat.pinnedText ? page.chat.pinnedText : ""; topMargin: 0; textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text) } }
            }
        }
        Container {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            layout: DockLayout {}
            ListView {
                id: list
                visible: !page.searching || found.size() === 0
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                dataModel: messagesModel
                layout: StackListLayout { orientation: LayoutOrientation.BottomToTop }
                scrollRole: ScrollRole.Main
                property variant st: store         // for the rows (MessageRow uses ListItem.view.st)
                function act(what, indexPath) {
                    var d = messagesModel.data(indexPath);
                    if (!d) return;
                    if (what === "reply") { page.editing = 0; page.replyTo = d.id; page.barText = "Reply to " + (d.out ? "your message" : d.sender); field.requestFocus(); }
                    else if (what === "copy") store.copyText(d.id);
                    else if (what === "forward") { picker.messageId = d.id; pickModel.clear(); pickModel.append(store.findChats("")); picker.open(); }
                    else if (what === "edit") { page.replyTo = 0; page.editing = d.id; page.barText = "Edit message"; field.text = store.plainText(d.id); field.requestFocus(); }
                    else if (what === "react") { page.reactTo = d.id; reactions.show(); }
                    else if (what === "pin") store.pinMessage(d.id, !d.pinned);
                    else if (what === "deleteAll") store.deleteMessage(d.id, true);
                    else if (what === "delete") store.deleteMessage(d.id, false);
                }
                onTriggered: {
                    var d = messagesModel.data(indexPath);
                    if (!d) return;
                    if (d.kind === "photo") { if (d.image !== "") page.showPhoto(d.image); }
                    else if (d.fileId > 0) {
                        if (d.fileState === "none") store.downloadFile(d.fileId);
                        else if (d.fileState !== "done") return;
                        else if (d.kind === "video" || d.kind === "animation" || d.kind === "videonote") page.showVideo(d.filePath);
                        else if (d.kind === "voice" || d.kind === "audio") store.playAudio(d.id);
                        else page.say("Downloaded: " + d.fileName);
                    }
                }
                listItemComponents: [
                    ListItemComponent { type: "in"; MessageRow { out: false } },
                    ListItemComponent { type: "out"; MessageRow { out: true } },
                    ListItemComponent { type: "service"; ServiceRow {} }
                ]
                attachedObjects: [
                    ListScrollStateHandler {
                        onAtEndChanged: if (atEnd) store.loadOlder()
                        onAtBeginningChanged: { page.scrolledUp = !atBeginning; if (atBeginning) store.loadNewer(); }
                    }
                ]
            }
            ListView {                             // search results
                visible: page.searching && found.size() > 0
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                dataModel: ArrayDataModel { id: found }
                function itemType(data, indexPath) { return "found"; }
                onTriggered: { var d = found.data(indexPath); if (d) { page.searching = false; store.jumpTo(d.msgId); } }
                listItemComponents: [ ListItemComponent { type: "found"; FoundRow {} } ]
            }
            Container {                            // back to the newest message
                visible: !page.searching && (page.scrolledUp || store.hasNewer)
                horizontalAlignment: HorizontalAlignment.Right; verticalAlignment: VerticalAlignment.Bottom
                rightPadding: ui.du(2); bottomPadding: ui.du(2)
                layout: DockLayout {}
                ImageView { imageSource: "asset:///icons/disc.png"; filterColor: Color.create(Skin.panelBg); preferredWidth: ui.du(11); preferredHeight: ui.du(11) }
                IconButton { icon: "asset:///icons/down.png"; horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center; onClicked: store.toLatest() }
            }
            ActivityIndicator {
                visible: store.loadingHistory && messagesModel.size() === 0
                running: visible
                horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                preferredWidth: ui.du(10); preferredHeight: ui.du(10)
            }
        }
        Container {                                // what the next message answers or changes
            visible: page.barText !== ""
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.create(Skin.panelBg)
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            leftPadding: ui.du(2)
            Container { background: Color.create(Skin.accent); minWidth: ui.du(0.6); preferredWidth: ui.du(0.6); verticalAlignment: VerticalAlignment.Fill }
            Label {
                text: page.barText
                leftMargin: ui.du(1.5)
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                verticalAlignment: VerticalAlignment.Center
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.link) }
            }
            IconButton { icon: "asset:///icons/close.png"; iconSize: ui.du(4.5); onClicked: { if (page.editing) field.text = ""; page.clearBar(); } }
        }
        Container {                                // recording a voice message
            visible: page.recording
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.create(Skin.panelBg)
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            leftPadding: ui.du(2.5); topPadding: ui.du(0.8); bottomPadding: ui.du(0.8)
            ImageView { imageSource: "asset:///icons/disc.png"; filterColor: Color.create(Skin.error); preferredWidth: ui.du(3); preferredHeight: ui.du(3); verticalAlignment: VerticalAlignment.Center }
            Label {
                text: Math.floor(page.recSecs / 60) + ":" + (page.recSecs % 60 < 10 ? "0" : "") + (page.recSecs % 60)
                leftMargin: ui.du(1.5); verticalAlignment: VerticalAlignment.Center
                textStyle { base: SystemDefaults.TextStyles.TitleText; color: Color.create(Skin.text) }
            }
            Label {
                text: "Cancel"
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.link); textAlign: TextAlign.Center }
                gestureHandlers: [ TapHandler { onTapped: page.stopRecording(false) } ]
            }
            IconButton { tint: Skin.accent; icon: "asset:///icons/send.png"; verticalAlignment: VerticalAlignment.Center; onClicked: page.stopRecording(true) }
        }
        Container {                                // input
            visible: page.chat.canSend === true && !page.recording
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.create(Skin.panelBg)
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            leftPadding: ui.du(0.5); topPadding: ui.du(0.8); bottomPadding: ui.du(0.8)
            IconButton { icon: "asset:///icons/attach.png"; verticalAlignment: VerticalAlignment.Bottom; onClicked: attachMenu.show() }
            TextArea {
                id: field
                hintText: "Message"
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                verticalAlignment: VerticalAlignment.Center
                maxHeight: ui.du(30)
                input { submitKey: SubmitKey.Send; onSubmitted: page.sendNow() }
                onTextChanging: { page.hasText = text !== ""; if (text !== "") store.typing(); }
                onTextChanged: page.hasText = text !== ""
            }
            IconButton { visible: page.keys && page.keys.length > 0; icon: "asset:///icons/keyboard.png"; verticalAlignment: VerticalAlignment.Bottom; onClicked: page.togglePanel("keyboard") }
            IconButton { visible: true; icon: page.panel === "media" ? "asset:///icons/keyboard.png" : "asset:///icons/sticker.png"; verticalAlignment: VerticalAlignment.Bottom; onClicked: { page.togglePanel("media"); if (page.panel === "") field.requestFocus(); } }
            IconButton { visible: !page.hasText; icon: "asset:///icons/mic.png"; verticalAlignment: VerticalAlignment.Bottom; onClicked: page.startRecording() }
            IconButton { tint: Skin.accent; visible: page.hasText; icon: "asset:///icons/send.png"; verticalAlignment: VerticalAlignment.Bottom; onClicked: page.sendNow() }
        }
        Container {                                // emoji / stickers / GIFs panel
            visible: page.panel === "media" && page.chat.canSend === true
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.create(Skin.listBg)
            preferredHeight: ui.du(50); minHeight: ui.du(50); maxHeight: ui.du(50)
            ScrollView {                           // top row: emoji categories or sticker sets
                horizontalAlignment: HorizontalAlignment.Fill
                preferredHeight: ui.du(7); minHeight: ui.du(7); maxHeight: ui.du(7)
                visible: page.mediaTab !== "gifs"
                scrollViewProperties { scrollMode: ScrollMode.Horizontal }
                Container {
                    id: chipBox
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    verticalAlignment: VerticalAlignment.Fill
                }
            }
            Container { horizontalAlignment: HorizontalAlignment.Fill; preferredHeight: 1; minHeight: 1; background: Color.create(Skin.line) }
            Container {
                layout: DockLayout {}
                horizontalAlignment: HorizontalAlignment.Fill
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                ListView {
                    id: grid
                    horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                    layout: GridListLayout { columnCount: page.mediaTab === "emoji" ? 8 : page.mediaTab === "gifs" ? 3 : 5; cellAspectRatio: page.mediaTab === "gifs" ? 1.3 : 1.0 }
                    dataModel: ArrayDataModel { id: gridModel }
                    listItemComponents: [ ListItemComponent {
                        Container {
                            id: cell
                            layout: DockLayout {}
                            horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                            background: cell.ListItem.active ? Color.create(Skin.press) : Color.Transparent
                            leftPadding: ui.du(0.4); rightPadding: ui.du(0.4); topPadding: ui.du(0.4); bottomPadding: ui.du(0.4)
                            ImageView { visible: ListItemData.image ? true : false; imageSource: ListItemData.image ? ListItemData.image : ""; scalingMethod: ScalingMethod.AspectFit
                                        horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill }
                            Label { visible: !ListItemData.image; text: ListItemData.e ? ListItemData.e : ""
                                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                                    textStyle { fontSize: FontSize.PointValue; fontSizeValue: 11 } }
                        }
                    } ]
                    onTriggered: {
                        var i = indexPath[0], d = gridModel.data(indexPath);
                        if (page.mediaTab === "emoji") { if (d && d.e) page.insertText(d.e); }
                        else if (page.mediaTab === "stickers") store.sendSticker(i);
                        else store.sendGif(i);
                    }
                }
                Label {
                    visible: page.gridCount === 0 && page.mediaTab !== "emoji"
                    text: page.mediaTab === "gifs" ? "No saved GIFs yet.\nGIFs you save in any Telegram app show up here." : store.stickerSet == 0 ? "No recent stickers yet" : "Loading…"
                    multiline: true
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                    textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.dim); textAlign: TextAlign.Center }
                }
            }
            Container {                            // bottom row: the three kinds + backspace
                horizontalAlignment: HorizontalAlignment.Fill
                preferredHeight: ui.du(7); minHeight: ui.du(7)
                background: Color.create(Skin.panelBg)
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                PanelTab { text: "Emoji"; on: page.mediaTab === "emoji"; onPicked: page.showTab("emoji") }
                PanelTab { text: "Stickers"; on: page.mediaTab === "stickers"; onPicked: page.showTab("stickers") }
                PanelTab { text: "GIFs"; on: page.mediaTab === "gifs"; onPicked: page.showTab("gifs") }
                IconButton { visible: page.mediaTab === "emoji"; icon: "asset:///icons/backspace.png"; verticalAlignment: VerticalAlignment.Center; onClicked: page.backspace() }
            }
        }
        Container {                                // bot keyboard
            id: keyBox
            visible: page.panel === "keyboard" && page.chat.canSend === true
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.create(Skin.listBg)
            leftPadding: ui.du(0.5); rightPadding: ui.du(1); topPadding: ui.du(0.5); bottomPadding: ui.du(1)
        }
        Container {
            visible: page.chat.canSend === false
            horizontalAlignment: HorizontalAlignment.Fill
            background: Color.create(Skin.panelBg)
            topPadding: ui.du(1); bottomPadding: ui.du(1)
            Button {
                visible: page.chat.canJoin === true
                text: page.chat.kind === "channel" ? "Join channel" : "Join group"
                horizontalAlignment: HorizontalAlignment.Center
                onClicked: store.joinChat(page.chat.id)
            }
            Label {
                visible: page.chat.canJoin !== true
                text: page.chat.secret === true ? (page.chat.secretState === "closed" ? "This secret chat has ended" : "The chat starts once they come online. Messages here are end-to-end encrypted and only on this phone.")
                    : page.chat.kind === "channel" ? (page.chat.muted ? "Unmute" : "Mute") : "You cannot write here"
                multiline: true
                horizontalAlignment: HorizontalAlignment.Center
                topMargin: ui.du(1.5); bottomMargin: ui.du(1.5)
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(page.chat.kind === "channel" ? Skin.accent : Skin.dim) }
                gestureHandlers: [ TapHandler { onTapped: if (page.chat.kind === "channel") store.setMuted(page.chat.id, !page.chat.muted) } ]
            }
        }
    }
    attachedObjects: [
        ComponentDefinition {
            id: chipDef
            Container {
                id: chip
                property variant key: 0
                property string text: ""
                property bool on: false
                property bool big: false
                signal picked(variant key)
                verticalAlignment: VerticalAlignment.Fill
                layout: DockLayout {}
                leftPadding: chip.big ? ui.du(1.6) : ui.du(2); rightPadding: chip.big ? ui.du(1.6) : ui.du(2)
                Container { visible: chip.on; horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Bottom
                            preferredHeight: ui.du(0.5); minHeight: ui.du(0.5); background: Color.create(Skin.accent) }
                Label { text: chip.text; verticalAlignment: VerticalAlignment.Center
                        textStyle { base: chip.big ? SystemDefaults.TextStyles.TitleText : SystemDefaults.TextStyles.SubtitleText
                                    color: Color.create(chip.on ? Skin.link : Skin.dim) } }
                gestureHandlers: [ TapHandler { onTapped: chip.picked(chip.key) } ]
            }
        },
        ComponentDefinition {
            id: keyLineDef
            Container {
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                horizontalAlignment: HorizontalAlignment.Fill; topMargin: ui.du(0.5)
            }
        },
        ComponentDefinition {
            id: keyDef
            Container {
                id: key
                property int row: 0
                property int col: 0
                property string text: ""
                property bool down: false
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                leftMargin: ui.du(0.5)
                topPadding: ui.du(1.6); bottomPadding: ui.du(1.6); leftPadding: ui.du(1); rightPadding: ui.du(1)
                background: Color.create(key.down ? Skin.keyDown : Skin.panelBg)
                onTouch: { if (event.isDown()) key.down = true; else if (event.isUp() || event.isCancel()) key.down = false; }
                gestureHandlers: [ TapHandler { onTapped: { store.pressKey(key.row, key.col); store.toLatest(); } } ]
                Label { text: key.text; horizontalAlignment: HorizontalAlignment.Center
                        textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text) } }
            }
        },
        QTimer { id: toBottom; interval: 60; singleShot: true; onTimeout: list.scrollToItem([0], ScrollAnimation.Smooth) },
        QTimer { id: recTick; interval: 500; onTimeout: page.recSecs = Math.floor((Date.now() - page.recStart) / 1000) },
        AudioRecorder {
            id: recorder
            onError: if (page.recording) { page.recording = false; store.discardFile(page.recUrl); page.say("Recording stopped (" + mediaError + ")."); }
        },
        MediaPlayer { id: player; onError: page.say("This recording cannot be played yet (" + mediaError + ").") },
        SystemListDialog {
            id: reactions
            title: "React"
            cancelButton.label: "Cancel"
            property variant emojis: ["👍", "❤", "😂", "😮", "😢", "🙏", "👎", "🔥", "🎉", "👏"]
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection && selectedIndices.length) store.react(page.reactTo, emojis[selectedIndices[0]])
        },
        SystemListDialog {
            id: attachMenu
            title: "Send"
            cancelButton.label: "Cancel"
            onFinished: {
                if (value !== SystemUiResult.ConfirmButtonSelection || !selectedIndices.length) return;
                filePicker.asMedia = selectedIndices[0] === 0;
                filePicker.type = filePicker.asMedia ? (FileType.Picture | FileType.Video) : FileType.Other | FileType.Document | FileType.Music | FileType.Picture | FileType.Video;
                filePicker.open();
            }
        },
        FilePicker {
            id: filePicker
            property bool asMedia: true
            mode: FilePickerMode.Picker
            title: asMedia ? "Photo or video" : "File"
            onFileSelected: { if (selectedFiles.length) { store.sendFile(selectedFiles[0], field.text, filePicker.asMedia); field.text = ""; store.toLatest(); } }
        },
        Sheet {
            id: picker
            property variant messageId: 0
            content: Page {
                Container {
                    background: Color.create(Skin.listBg)
                    TopBar { title: "Forward to"; firstIcon: "close"; onFirstClicked: picker.close() }
                    ChatList {
                        endless: false
                        dataModel: ArrayDataModel { id: pickModel }
                        onPicked: { store.forwardMessage(picker.messageId, chat.id); picker.close(); page.say("Forwarded to " + chat.title); }
                    }
                }
            }
        }
    ]
}
