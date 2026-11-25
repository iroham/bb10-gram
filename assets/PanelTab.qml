// PanelTab: one tab of the emoji/sticker/GIF panel's bottom row.
import bb.cascades 1.3
import gram 1.0

Container {
    id: tab
    property string text: ""
    property bool on: false
    signal picked()
    layoutProperties: StackLayoutProperties { spaceQuota: 1 }
    verticalAlignment: VerticalAlignment.Fill
    layout: DockLayout {}
    Container { visible: tab.on; horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Top
                preferredHeight: ui.du(0.5); minHeight: ui.du(0.5); background: Color.create(Skin.accent) }
    Label { text: tab.text; horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
            textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(tab.on ? Skin.link : Skin.dim); fontWeight: tab.on ? FontWeight.W500 : FontWeight.Normal } }
    gestureHandlers: [ TapHandler { onTapped: tab.picked() } ]
}
