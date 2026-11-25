// EditChatPage: a group's or channel's photo, name and description (for admins who may change the info).
import bb.cascades 1.3
import bb.system 1.2
import bb.cascades.pickers 1.0
import gram 1.0

Page {
    id: page
    signal goBack()
    property variant p: store.profile
    actionBarVisibility: ChromeVisibility.Hidden
    onCreationCompleted: { nameField.text = p.title ? p.title : ""; aboutField.text = p.description ? p.description : ""; }
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: page.p.isChannel ? "Edit channel" : "Edit group"; firstIcon: "done"
                 onBackClicked: page.goBack(); onFirstClicked: { store.saveChatInfo(page.p.id, nameField.text, aboutField.text); page.goBack(); } }
        ScrollView {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                leftPadding: ui.du(3); rightPadding: ui.du(3); topPadding: ui.du(3); bottomPadding: ui.du(4)
                Avatar {
                    horizontalAlignment: HorizontalAlignment.Center
                    photo: page.p.photo ? page.p.photo : ""; initials: page.p.initials ? page.p.initials : ""; tint: page.p.color ? page.p.color : "#65aadd"
                    size: ui.du(24)
                    gestureHandlers: [ TapHandler { onTapped: picker.open() } ]
                }
                Label {
                    horizontalAlignment: HorizontalAlignment.Center
                    text: "Tap the picture to change it"
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.accent) }
                    gestureHandlers: [ TapHandler { onTapped: picker.open() } ]
                }
                Label { topMargin: ui.du(3); text: "Name"; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.accent) } }
                TextField { id: nameField; hintText: page.p.isChannel ? "Channel name" : "Group name" }
                Label { topMargin: ui.du(3); text: "Description"; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.accent) } }
                TextArea { id: aboutField; hintText: "Optional"; preferredHeight: ui.du(24) }
            }
        }
    }
    attachedObjects: [
        FilePicker {
            id: picker
            type: FileType.Picture
            title: "New photo"
            onFileSelected: if (selectedFiles.length) store.setChatPhoto(page.p.id, selectedFiles[0])
        }
    ]
}
