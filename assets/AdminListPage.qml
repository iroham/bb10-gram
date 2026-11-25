// AdminListPage: "Removed users" (tap one to let them join again) or "Recent actions" (the admin log, 100 newest).
import bb.cascades 1.3
import bb.system 1.2
import gram 1.0

Page {
    id: page
    signal goBack()
    property string kind: "banned"            // banned | log
    property variant chatId: 0
    property bool loading: true
    actionBarVisibility: ChromeVisibility.Hidden
    function load() {
        page.loading = true;
        if (page.kind === "banned") store.loadBanned(page.chatId); else store.loadEventLog(page.chatId);
    }
    function got(k, rows) {
        if (k !== page.kind) return;
        page.loading = false;
        model.clear();
        model.append(rows);
    }
    onCreationCompleted: store.adminList.connect(page.got)
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: page.kind === "banned" ? "Removed users" : "Recent actions"; onBackClicked: page.goBack() }
        Label {
            visible: !page.loading && model.size() === 0
            horizontalAlignment: HorizontalAlignment.Center
            topMargin: ui.du(6)
            multiline: true
            text: page.kind === "banned" ? "Nobody has been removed." : "No actions in the last 48 hours."
            textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.dim); textAlign: TextAlign.Center }
        }
        ActivityIndicator { visible: page.loading; running: page.loading; horizontalAlignment: HorizontalAlignment.Center; topMargin: ui.du(6) }
        ListView {
            id: list
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            dataModel: ArrayDataModel { id: model }
            onTriggered: {
                if (page.kind !== "banned") return;
                var r = model.data(indexPath);
                unbanDlg.userId = r.id; unbanDlg.body = "Let " + r.title + " join again? They are not added back; they can join with a link or be added.";
                unbanDlg.show();
            }
            listItemComponents: [
                ListItemComponent {
                    Container {
                        id: item
                        horizontalAlignment: HorizontalAlignment.Fill
                        leftPadding: ui.du(3); rightPadding: ui.du(3); topPadding: ui.du(1.6); bottomPadding: ui.du(1.6)
                        Label { text: ListItemData.title ? ListItemData.title : ListItemData.text; multiline: true
                                textStyle { base: SystemDefaults.TextStyles.PrimaryText; color: Color.create(Skin.text) } }
                        Label { text: ListItemData.date ? ListItemData.date : ListItemData.preview; topMargin: 0
                                textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) } }
                        Container { topMargin: ui.du(1.6); horizontalAlignment: HorizontalAlignment.Fill; preferredHeight: 1; minHeight: 1; background: Color.create(Skin.line) }
                    }
                }
            ]
        }
    }
    attachedObjects: [
        SystemDialog {
            id: unbanDlg
            property variant userId: 0
            title: "Remove from the list"
            confirmButton.label: "Allow"
            cancelButton.label: "Cancel"
            onFinished: if (value === SystemUiResult.ConfirmButtonSelection) { store.unbanMember(page.chatId, unbanDlg.userId); page.load(); }
        }
    ]
}
