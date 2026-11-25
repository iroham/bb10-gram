// ArchivePage: the archived chats.
import bb.cascades 1.3
import gram 1.0

Page {
    id: page
    signal goBack()
    signal picked(variant chat)
    actionBarVisibility: ChromeVisibility.Hidden
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: "Archived chats"; onBackClicked: page.goBack() }
        ChatList { listKey: "archive"; dataModel: archiveModel; onPicked: page.picked(chat) }
    }
}
