// CallButton: a round call-screen button with its label underneath. filled = coloured disc (End, Accept,
// Decline); otherwise a translucent disc that turns white while "on" (Mute, Speaker).
import bb.cascades 1.3

Container {
    id: cb
    property string icon: ""
    property string label: ""
    property string tint: "#ffffffff"
    property bool filled: false
    property bool on: false
    signal clicked()
    Container {
        id: disc
        layout: DockLayout {}
        horizontalAlignment: HorizontalAlignment.Center
        preferredWidth: ui.du(15); preferredHeight: ui.du(15); minWidth: ui.du(15); minHeight: ui.du(15)
        ImageView {
            imageSource: "asset:///icons/disc.png"
            filterColor: Color.create(cb.filled ? cb.tint : cb.on ? "#ffffffff" : "#33ffffff")
            horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
        }
        ImageView {
            imageSource: cb.icon
            filterColor: Color.create(!cb.filled && cb.on ? "#ff0e1621" : "#ffffffff")
            preferredWidth: ui.du(7.5); preferredHeight: ui.du(7.5)
            horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
        }
        onTouch: {
            if (event.isDown()) disc.opacity = 0.7;
            else if (event.isUp()) { disc.opacity = 1; cb.clicked(); }
            else if (event.isCancel()) disc.opacity = 1;
        }
    }
    Label {
        text: cb.label
        horizontalAlignment: HorizontalAlignment.Center
        textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create("#ffb8c7d6") }
    }
}
