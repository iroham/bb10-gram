// ServiceRow: a line in the middle of the chat ("Anna joined the group"), with the day line above if needed.
import bb.cascades 1.3
import gram 1.0

Container {
    horizontalAlignment: HorizontalAlignment.Fill
    topPadding: ui.du(0.6); bottomPadding: ui.du(0.6)
    Container {
        visible: ListItemData.day !== ""
        horizontalAlignment: HorizontalAlignment.Center
        topMargin: ui.du(1.5); bottomMargin: ui.du(1.5)
        leftPadding: ui.du(2); rightPadding: ui.du(2); topPadding: ui.du(0.5); bottomPadding: ui.du(0.5)
        background: Color.create(Skin.dayChip)
        Label { text: ListItemData.day; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.text); fontWeight: FontWeight.W500 } }
    }
    Container {
        horizontalAlignment: HorizontalAlignment.Center
        maxWidth: ui.du(100)
        leftPadding: ui.du(2); rightPadding: ui.du(2); topPadding: ui.du(0.5); bottomPadding: ui.du(0.5)
        background: Color.create(Skin.dayChip)
        Label { text: ListItemData.text; multiline: true; textFormat: TextFormat.Html; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.text); textAlign: TextAlign.Center } }
    }
}
