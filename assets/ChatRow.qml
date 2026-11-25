// ChatRow: one line of a chat list. ListItemData = Store::chatRow().
import bb.cascades 1.3
import gram 1.0

Container {
    id: row
    horizontalAlignment: HorizontalAlignment.Fill
    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
    leftPadding: ui.du(2); topPadding: ui.du(1.2); bottomPadding: ui.du(1.2)
    Avatar {
        photo: ListItemData.photo; initials: ListItemData.initials; tint: ListItemData.color
        verticalAlignment: VerticalAlignment.Center
    }
    Container {
        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
        verticalAlignment: VerticalAlignment.Center
        leftPadding: ui.du(2); rightPadding: ui.du(2.5); topPadding: ui.du(0.6); bottomPadding: ui.du(0.6)
        background: row.ListItem.active ? Color.create(Skin.press) : Color.Transparent
        Container {
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            ImageView {
                visible: ListItemData.kind === "group" || ListItemData.kind === "channel"
                imageSource: ListItemData.kind === "channel" ? "asset:///icons/megaphone.png" : "asset:///icons/group.png"
                preferredWidth: ui.du(4); preferredHeight: ui.du(4); minWidth: ui.du(4)
                verticalAlignment: VerticalAlignment.Center
                rightMargin: ui.du(0.8)
            }
            ImageView {
                visible: ListItemData.secret === true
                imageSource: "asset:///icons/lock.png"
                filterColor: Color.create(Skin.secret)
                preferredWidth: ui.du(3.8); preferredHeight: ui.du(3.8); minWidth: ui.du(3.8)
                verticalAlignment: VerticalAlignment.Center
                rightMargin: ui.du(0.6)
            }
            Label {
                text: ListItemData.title
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                verticalAlignment: VerticalAlignment.Center
                textStyle { base: SystemDefaults.TextStyles.PrimaryText; color: Color.create(ListItemData.secret === true ? Skin.secret : "#ffffffff"); fontWeight: FontWeight.W500 }
            }
            ImageView {
                visible: ListItemData.muted
                imageSource: "asset:///icons/muted.png"
                filterColor: Color.create(Skin.dim)
                preferredWidth: ui.du(3.6); preferredHeight: ui.du(3.6); minWidth: ui.du(3.6)
                verticalAlignment: VerticalAlignment.Center
            }
            ImageView {
                visible: ListItemData.state !== ""
                imageSource: "asset:///icons/" + (ListItemData.state === "" ? "sent" : ListItemData.state) + ".png"
                filterColor: Color.create(ListItemData.state === "failed" ? Skin.error : Skin.accent)
                preferredWidth: ui.du(4); preferredHeight: ui.du(4); minWidth: ui.du(4)
                verticalAlignment: VerticalAlignment.Center
                leftMargin: ui.du(1)
            }
            Label {
                text: ListItemData.date
                verticalAlignment: VerticalAlignment.Center
                leftMargin: ui.du(1)
                textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(ListItemData.unread > 0 && !ListItemData.muted ? Skin.accent : Skin.dim) }
            }
            ImageView {                        // picked (new group)
                visible: ListItemData.selected === true
                imageSource: "asset:///icons/done.png"
                filterColor: Color.create(Skin.accent)
                preferredWidth: ui.du(4.5); preferredHeight: ui.du(4.5); minWidth: ui.du(4.5)
                verticalAlignment: VerticalAlignment.Center
            }
        }
        Container {
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            topMargin: ui.du(0.4)
            Label {
                text: ListItemData.typing ? ListItemData.typing : (ListItemData.draft ? "Draft: " : "") + ListItemData.preview
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                verticalAlignment: VerticalAlignment.Center
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(ListItemData.typing ? Skin.link : ListItemData.draft ? Skin.error : Skin.dim) }
            }
            ImageView {
                visible: ListItemData.pinned && ListItemData.unread === 0 && !ListItemData.marked
                imageSource: "asset:///icons/pin.png"
                filterColor: Color.create(Skin.dim)
                preferredWidth: ui.du(4); preferredHeight: ui.du(4); minWidth: ui.du(4)
                verticalAlignment: VerticalAlignment.Center
            }
            Label {
                visible: ListItemData.mentions
                text: "@"
                verticalAlignment: VerticalAlignment.Center
                rightMargin: ui.du(1)
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.accent); fontWeight: FontWeight.Bold }
            }
            Container {
                visible: ListItemData.unread > 0 || ListItemData.marked
                layout: DockLayout {}
                verticalAlignment: VerticalAlignment.Center
                minWidth: ui.du(5.4); minHeight: ui.du(5.4); preferredHeight: ui.du(5.4)
                ImageView {
                    imageSource: "asset:///icons/disc.png"
                    filterColor: Color.create(ListItemData.muted ? Skin.badgeMuted : Skin.badge)
                    preferredWidth: ui.du(5.4); preferredHeight: ui.du(5.4)
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                }
                Label {
                    text: ListItemData.unread > 999 ? "999+" : ListItemData.unread > 0 ? "" + ListItemData.unread : ""
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.White; fontWeight: FontWeight.W500
                                fontSize: ListItemData.unread > 99 ? FontSize.XXSmall : FontSize.XSmall }
                }
            }
        }
    }
    contextActions: [
        ActionSet {
            title: ListItemData.title
            ActionItem { title: "Mark as read"; imageSource: "asset:///icons/done.png"; enabled: ListItemData.unread > 0 || ListItemData.marked
                         onTriggered: row.ListItem.view.act("read", row.ListItem.indexPath) }
            ActionItem { title: ListItemData.muted ? "Unmute" : "Mute"; imageSource: ListItemData.muted ? "asset:///icons/bell.png" : "asset:///icons/muted.png"
                         onTriggered: row.ListItem.view.act("mute", row.ListItem.indexPath) }
            ActionItem { title: ListItemData.pinned ? "Unpin" : "Pin"; imageSource: "asset:///icons/pin.png"
                         onTriggered: row.ListItem.view.act("pin", row.ListItem.indexPath) }
            ActionItem { title: ListItemData.archived ? "Unarchive" : "Archive"; imageSource: "asset:///icons/archive.png"
                         onTriggered: row.ListItem.view.act("archive", row.ListItem.indexPath) }
            DeleteActionItem { title: (ListItemData.kind === "group" || ListItemData.kind === "channel") ? "Leave" : "Delete chat"
                         onTriggered: row.ListItem.view.act("delete", row.ListItem.indexPath) }
        }
    ]
}
