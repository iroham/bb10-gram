// ChatList: a list of chats (main list, archive, search results, forward target picker).
import bb.cascades 1.3
import gram 1.0
import bb.system 1.2

ListView {
    id: lv
    property string listKey: "main"
    property bool endless: true            // ask the store for more at the end (not for search results)
    signal picked(variant chat)
    scrollRole: ScrollRole.Main
    function itemType(data, indexPath) { return data.type === "header" ? "header" : data.type === "message" ? "found" : "chat"; }
    function act(what, indexPath) {
        var d = lv.dataModel.data(indexPath);
        if (!d) return;
        if (what === "read") store.markRead(d.id);
        else if (what === "mute") { if (d.muted) store.setMuteFor(d.id, 0); else muteFor.ask(d.id); }
        else if (what === "pin") store.setPinned(d.id, !d.pinned);
        else if (what === "archive") store.setArchived(d.id, !d.archived);
        else if (what === "delete") { confirm.chatId = d.id; confirm.title = (d.kind === "group" || d.kind === "channel") ? "Leave " + d.title + "?" : "Delete the chat with " + d.title + "?"; confirm.show(); }
    }
    onTriggered: { var d = lv.dataModel.data(indexPath); if (d && d.type !== "header") lv.picked(d); }
    listItemComponents: [
        ListItemComponent { type: "chat"; ChatRow {} },
        ListItemComponent { type: "found"; FoundRow {} },
        ListItemComponent {
            type: "header"
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                background: Color.create(Skin.keyBg)
                leftPadding: ui.du(2.5); topPadding: ui.du(1); bottomPadding: ui.du(1)
                Label { text: ListItemData.title; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.accent); fontWeight: FontWeight.W500 } }
            }
        }
    ]
    attachedObjects: [
        SystemListDialog {
            id: muteFor
            property variant chatId: 0
            property variant secs: [3600, 8 * 3600, 2 * 86400, 7 * 86400, 0x7fffffff]
            title: "Mute notifications"
            cancelButton.label: "Cancel"
            function ask(id) {
                muteFor.chatId = id; muteFor.clearList();
                var t = ["For 1 hour", "For 8 hours", "For 2 days", "For 1 week", "Until I unmute"];
                for (var i = 0; i < t.length; i++) muteFor.appendItem(t[i]);
                muteFor.show();
            }
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection && selectedIndices.length) store.setMuteFor(muteFor.chatId, Math.min(secs[selectedIndices[0]], 366 * 86400))
        },
        ListScrollStateHandler { onAtEndChanged: if (atEnd && lv.endless) store.loadMoreChats(lv.listKey) },
        SystemDialog {
            id: confirm
            property variant chatId: 0
            body: "The messages are removed from this account's chat list on all your devices."
            confirmButton.label: "Yes"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection) store.deleteChat(confirm.chatId)
        }
    ]
}
