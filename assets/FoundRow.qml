// FoundRow: a message found by search: chat, sender and text, date.
import bb.cascades 1.3
import gram 1.0

Container {
    id: row
    horizontalAlignment: HorizontalAlignment.Fill
    background: row.ListItem.active ? Color.create(Skin.press) : Color.Transparent
    leftPadding: ui.du(2.5); rightPadding: ui.du(2.5); topPadding: ui.du(1.4); bottomPadding: ui.du(1.4)
    Container {
        layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
        Label {
            text: ListItemData.title
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            textStyle { base: SystemDefaults.TextStyles.PrimaryText; color: Color.create(Skin.text); fontWeight: FontWeight.W500 }
        }
        Label { text: ListItemData.date; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) } }
    }
    Label {
        text: ListItemData.preview
        topMargin: ui.du(0.3)
        textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.dim) }
    }
}
