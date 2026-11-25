// VideoPage: plays a downloaded video, GIF or round video full screen. Tap: pause/play; back returns.
import bb.cascades 1.3
import bb.multimedia 1.0

Page {
    id: page
    property string path: ""
    property bool loop: false
    signal goBack()
    signal say(string text)
    actionBarVisibility: ChromeVisibility.Hidden
    function start() { player.sourceUrl = page.path; player.play(); }
    function leaving() { player.stop(); }
    Container {
        layout: DockLayout {}
        background: Color.Black
        ForeignWindowControl {
            id: video
            windowId: "gramVideo"
            updatedProperties: WindowProperty.Position | WindowProperty.Size | WindowProperty.Visible
            visible: boundToWindow
            horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
            gestureHandlers: [ TapHandler { onTapped: { if (player.mediaState === MediaState.Started) player.pause(); else player.play(); } } ]
        }
        IconButton { tint: "#ffffffff"; icon: "asset:///icons/back.png"; horizontalAlignment: HorizontalAlignment.Left; verticalAlignment: VerticalAlignment.Top; onClicked: page.goBack() }
        ImageView {
            visible: player.mediaState === MediaState.Paused
            imageSource: "asset:///icons/play.png"
            preferredWidth: ui.du(14); preferredHeight: ui.du(14)
            horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
        }
    }
    attachedObjects: [
        MediaPlayer {
            id: player
            windowId: video.windowId
            windowGroupId: video.windowGroup
            videoOutput: VideoOutput.PrimaryDisplay
            repeatMode: page.loop ? RepeatMode.Track : RepeatMode.None
            onError: page.say("This video cannot be played on this phone (" + mediaError + ").")
        }
    ]
}
