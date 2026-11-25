// ContactsPage: the account's contacts. mode "message": tap opens a chat. mode "group": tap picks people, the
// tick creates the group (asks for its name). mode "channel": only asks for the name and description.
import bb.cascades 1.3
import bb.system 1.2
import gram 1.0

Page {
    id: page
    property variant all: []
    property string mode: "message"
    property variant picked: []            // user ids
    property variant targetChat: 0         // mode "add": the group to add the picked people to
    signal goBack()
    actionBarVisibility: ChromeVisibility.Hidden
    signal openMode(string mode)
    attachedObjects: [
        SystemPrompt {
            id: namePrompt
            title: page.mode === "channel" ? "Channel name" : "Group name"
            inputField.emptyText: page.mode === "channel" ? "Name" : "Name of the group"
            confirmButton.label: "Create"
            cancelButton.label: "Cancel"
            onFinished: {
                if (value !== SystemUiResult.ConfirmButtonSelection) { if (page.mode === "channel") page.goBack(); return; }
                var name = inputFieldTextEntry().replace(/^\s+|\s+$/g, "");
                if (name === "") { namePrompt.show(); return; }
                store.createChat(name, page.picked, page.mode === "channel");
            }
        }
    ]
    function onFound(kind, rows) { if (kind === "contacts") { page.all = rows; page.filter(query.text); } }
    function filter(text) {
        rows.clear();
        var t = text.toLowerCase(), out = [];
        for (var i = 0; i < page.all.length; i++) if (t === "" || page.all[i].title.toLowerCase().indexOf(t) >= 0) {
            var r = page.all[i]; r.selected = page.picked.indexOf(r.id) >= 0; out.push(r);
        }
        if (out.length) rows.append(out);
    }
    function leaving() { store.searchDone.disconnect(page.onFound); }
    function toggle(chat) {
        var l = page.picked.slice(0), i = l.indexOf(chat.id);
        if (i >= 0) l.splice(i, 1); else l.push(chat.id);
        page.picked = l;
        for (var k = 0; k < rows.size(); k++) {
            var d = rows.data([k]);
            if (d.id === chat.id) { d.selected = i < 0; rows.replace(k, d); break; }
        }
    }
    onCreationCompleted: { store.searchDone.connect(page.onFound); if (page.mode !== "channel") store.loadContacts(); else namePrompt.show(); }
    Container {
        background: Color.create(Skin.listBg)
        TopBar {
            back: true
            title: page.mode === "add" ? "Add members" : page.mode === "group" ? "New group" : page.mode === "channel" ? "New channel" : "New message"
            subtitle: page.mode === "group" || page.mode === "add" ? (page.picked.length ? page.picked.length + " of " + page.all.length + " picked" : "Pick people to add") : page.mode === "channel" ? "" : page.all.length + " contacts"
            secondIcon: page.mode === "message" ? "" : "done"
            onSecondClicked: { if (page.mode === "add") { if (page.picked.length) { store.addMembers(page.targetChat, page.picked); page.goBack(); } } else namePrompt.show(); }
            onBackClicked: page.goBack()
        }
        Container {
            visible: page.mode === "message"
            OptionRow { icon: "group"; title: "New group"; onClicked: page.openMode("group") }
            OptionRow { icon: "megaphone"; title: "New channel"; onClicked: page.openMode("channel") }
        }
        Container {
            leftPadding: ui.du(2); rightPadding: ui.du(2); topPadding: ui.du(1.5); bottomPadding: ui.du(1)
            TextField { id: query; hintText: "Search contacts"; onTextChanging: page.filter(text) }
        }
        ChatList {
            endless: false
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            dataModel: ArrayDataModel { id: rows }
            visible: page.mode !== "channel"
            onPicked: { if (page.mode === "group" || page.mode === "add") page.toggle(chat); else store.openUserChat(chat.id); }
        }
    }
}
