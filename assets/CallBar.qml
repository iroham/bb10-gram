// CallBar: the green strip at the top of the chat list and chats while a call goes on behind a minimized
// call screen. Tap it to go back to the call.
import bb.cascades 1.3

Container {
    id: bar
    property variant c: store.calls
    visible: c.active && !nav.callOpen
    horizontalAlignment: HorizontalAlignment.Fill
    background: Color.create(c.phase === "ended" ? "#ffb04a45" : "#ff3a9d4a")
    leftPadding: ui.du(2); rightPadding: ui.du(2); topPadding: ui.du(1); bottomPadding: ui.du(1)
    layout: DockLayout {}
    Container {
        layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
        verticalAlignment: VerticalAlignment.Center
        ImageView { imageSource: "asset:///icons/call.png"; preferredWidth: ui.du(3); preferredHeight: ui.du(3); verticalAlignment: VerticalAlignment.Center }
        Label {
            leftMargin: ui.du(1); verticalAlignment: VerticalAlignment.Center
            text: bar.c.name + (bar.c.phase === "ended" ? " · " + bar.c.status : bar.c.muted ? " · muted" : "")
            textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.White; fontWeight: FontWeight.W500 }
        }
    }
    Label {
        horizontalAlignment: HorizontalAlignment.Right; verticalAlignment: VerticalAlignment.Center
        text: bar.c.phase === "talking" ? nav.clock(bar.c.seconds) : bar.c.phase === "ended" ? "" : bar.c.status
        textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.White }
        attachedObjects: [ ImplicitAnimationController { enabled: false } ]
    }
    gestureHandlers: [ TapHandler { onTapped: nav.openCall() } ]
}
