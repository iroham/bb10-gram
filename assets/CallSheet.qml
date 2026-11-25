// CallSheet: the full-screen call screen (store.calls). Photo, name and the call's state in the middle; at the
// bottom Decline / Accept while it rings for us, otherwise Mute, Speaker and End. Once connected, the four key
// emoji show that the call is end-to-end encrypted (tap them for what they mean). The arrow at the top left
// minimizes the screen (the call goes on; a green bar in the app brings it back). Held to the ear, the screen
// goes black and ignores touches.
import bb.cascades 1.3
import bb.system 1.2
import gram 1.0

Sheet {
    id: sheet
    property variant c: store.calls
    peekEnabled: false
    function talking() { return c.phase === "talking" }
    function clock(s) {
        var h = Math.floor(s / 3600), m = Math.floor(s / 60) % 60, x = s % 60;
        return (h > 0 ? h + ":" + (m < 10 ? "0" : "") : "") + (m < 10 && h === 0 ? "0" : "") + m + ":" + (x < 10 ? "0" : "") + x;
    }
    Page {
        Container {
            layout: DockLayout {}
            background: Color.create("#ff0e1621")
            horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill

            Container {                                // top: minimize, encryption note, signal
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Top
                layout: DockLayout {}
                topPadding: ui.du(1); leftPadding: ui.du(1); rightPadding: ui.du(2.4)
                IconButton {
                    tint: "#ffffffff"
                    icon: "asset:///icons/down.png"
                    visible: !sheet.c.incoming
                    horizontalAlignment: HorizontalAlignment.Left
                    onClicked: sheet.close()
                }
                Container {
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    ImageView { imageSource: "asset:///icons/lock.png"; filterColor: Color.create(Skin.dim)
                                preferredWidth: ui.du(2.6); preferredHeight: ui.du(2.6); verticalAlignment: VerticalAlignment.Center }
                    Label { text: "End-to-end encrypted"; leftMargin: ui.du(0.6); verticalAlignment: VerticalAlignment.Center
                            textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) } }
                }
                Container {                            // signal: four bars
                    visible: sheet.c.phase === "talking" || sheet.c.phase === "reconnecting"
                    horizontalAlignment: HorizontalAlignment.Right; verticalAlignment: VerticalAlignment.Center
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    Container { verticalAlignment: VerticalAlignment.Bottom; preferredWidth: ui.du(0.8); preferredHeight: ui.du(1.0); minHeight: ui.du(1.0)
                                background: Color.create(sheet.c.bars >= 1 ? "#ffffffff" : "#40ffffff") }
                    Container { leftMargin: ui.du(0.4); verticalAlignment: VerticalAlignment.Bottom; preferredWidth: ui.du(0.8); preferredHeight: ui.du(1.6); minHeight: ui.du(1.6)
                                background: Color.create(sheet.c.bars >= 2 ? "#ffffffff" : "#40ffffff") }
                    Container { leftMargin: ui.du(0.4); verticalAlignment: VerticalAlignment.Bottom; preferredWidth: ui.du(0.8); preferredHeight: ui.du(2.2); minHeight: ui.du(2.2)
                                background: Color.create(sheet.c.bars >= 3 ? "#ffffffff" : "#40ffffff") }
                    Container { leftMargin: ui.du(0.4); verticalAlignment: VerticalAlignment.Bottom; preferredWidth: ui.du(0.8); preferredHeight: ui.du(2.8); minHeight: ui.du(2.8)
                                background: Color.create(sheet.c.bars >= 4 ? "#ffffffff" : "#40ffffff") }
                }
            }

            Container {                                // middle: who and what
                horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Top
                topPadding: ui.du(12)
                Avatar {
                    horizontalAlignment: HorizontalAlignment.Center
                    size: ui.du(30); big: true; mask: "chat"
                    photo: sheet.c.photo; initials: sheet.c.initials; tint: sheet.c.color !== "" ? sheet.c.color : "#65aadd"
                }
                Label {
                    text: sheet.c.name
                    horizontalAlignment: HorizontalAlignment.Center
                    topMargin: ui.du(3)
                    textStyle { base: SystemDefaults.TextStyles.BigText; color: Color.White; fontWeight: FontWeight.W500 }
                }
                Label {
                    id: statusLine
                    text: sheet.talking() ? sheet.clock(sheet.c.seconds) : sheet.c.status
                    horizontalAlignment: HorizontalAlignment.Center
                    topMargin: 0
                    textStyle { base: SystemDefaults.TextStyles.TitleText
                                color: Color.create(sheet.c.phase === "ended" ? "#ffe0605a" : sheet.c.phase === "reconnecting" ? "#ffeda86c" : Skin.dim) }
                    attachedObjects: [ ImplicitAnimationController { enabled: false } ]
                }
                Label {                                // the key emoji (same on both phones = nobody in between)
                    visible: sheet.c.emojis !== "" && sheet.c.phase !== "ended"
                    text: sheet.c.emojis
                    horizontalAlignment: HorizontalAlignment.Center
                    topMargin: ui.du(2)
                    textStyle { fontSize: FontSize.PointValue; fontSizeValue: 14 }
                    gestureHandlers: [ TapHandler { onTapped: keyInfo.show() } ]
                }
            }

            Container {                                // bottom: ringing for us
                visible: sheet.c.incoming
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Bottom
                bottomPadding: ui.du(8); leftPadding: ui.du(14); rightPadding: ui.du(14)
                layout: DockLayout {}
                CallButton { horizontalAlignment: HorizontalAlignment.Left; icon: "asset:///icons/callend.png"; tint: "#ffe0605a"; filled: true
                             label: "Decline"; onClicked: sheet.c.hangUp() }
                CallButton { horizontalAlignment: HorizontalAlignment.Right; icon: "asset:///icons/call.png"; tint: "#ff4fbf5c"; filled: true
                             label: "Accept"; onClicked: sheet.c.accept() }
            }
            Container {                                // bottom: during the call
                visible: !sheet.c.incoming && sheet.c.phase !== "ended"
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Bottom
                bottomPadding: ui.du(8); leftPadding: ui.du(8); rightPadding: ui.du(8)
                layout: DockLayout {}
                CallButton { horizontalAlignment: HorizontalAlignment.Left; icon: "asset:///icons/micoff.png"; on: sheet.c.muted
                             label: "Mute"; onClicked: sheet.c.setMuted(!sheet.c.muted) }
                CallButton { horizontalAlignment: HorizontalAlignment.Center; icon: "asset:///icons/callend.png"; tint: "#ffe0605a"; filled: true
                             label: "End"; onClicked: sheet.c.hangUp() }
                CallButton { horizontalAlignment: HorizontalAlignment.Right; icon: "asset:///icons/speaker.png"; on: sheet.c.speaker
                             label: "Speaker"; onClicked: sheet.c.setSpeaker(!sheet.c.speaker) }
            }

            Container {                                // held to the ear: black, swallows touches
                visible: sheet.c.near
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                background: Color.Black
                onTouch: { }
            }
        }
    }
    attachedObjects: [
        SystemDialog {
            id: keyInfo
            title: "Encryption key"
            body: "If these emoji are the same on " + sheet.c.name + "'s screen, this call is end-to-end encrypted and nobody can listen in."
            cancelButton.label: undefined
        }
    ]
}
