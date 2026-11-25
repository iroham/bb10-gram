// StoryViewer: full-screen stories. Bars on top show progress; tap the left third = back, elsewhere = next;
// hold = pause; swipe down = close. Under the story: a private reply and a heart (when the poster allows replies).
import bb.cascades 1.3
import bb.multimedia 1.0
import gram 1.0

Sheet {
    id: sheet
    peekEnabled: false
    property variant s: store.story
    property string key: ""
    property real elapsed: 0
    property bool held: false
    property real w: 720
    property real downY: -1
    property bool shown: false
    function start(chatId) {
        sheet.key = ""; sheet.elapsed = 0;
        store.openStories(chatId);
        sheet.open(); sheet.shown = true;
        tick.start();
    }
    function finish() {
        tick.stop(); player.stop(); sheet.shown = false;
        store.closeStories();
        sheet.close();
    }
    function next() { player.stop(); if (!store.nextStory()) sheet.finish(); }
    function prev() { player.stop(); if (!store.prevStory()) { sheet.elapsed = 0; if (sheet.s.kind === "video") player.seekTime(0); } }
    // paused while the picture/video isn't here yet, while held, while typing a reply
    function running() {
        if (sheet.held || reply.focused) return false;
        if (sheet.s.loading) return false;
        if (sheet.s.kind === "photo") return sheet.s.image !== "";
        if (sheet.s.kind === "video") return player.mediaState === MediaState.Started;
        return true;
    }
    onSChanged: {
        var k = s.chatId + ":" + s.id;
        if (k !== sheet.key) { sheet.key = k; sheet.elapsed = 0; player.stop(); reply.text = ""; }
        if (s.kind === "video" && s.video && s.video !== "" && player.mediaState !== MediaState.Started && sheet.shown) {
            player.sourceUrl = s.video; player.play();
        }
    }
    Page {
        Container {
            id: root
            layout: DockLayout {}
            background: Color.Black
            attachedObjects: [ LayoutUpdateHandler { onLayoutFrameChanged: sheet.w = layoutFrame.width } ]
            onTouch: {
                if (event.isDown()) { sheet.downY = event.windowY; holdTimer.start(); }
                else if (event.isUp() || event.isCancel()) {
                    holdTimer.stop();
                    var wasHeld = sheet.held; sheet.held = false;
                    if (event.isUp() && sheet.downY >= 0 && event.windowY - sheet.downY > ui.du(20)) { sheet.downY = -1; sheet.finish(); return; }
                    sheet.downY = -1;
                    if (wasHeld) return;
                }
            }
            gestureHandlers: [ TapHandler { onTapped: { if (event.x < sheet.w / 3) sheet.prev(); else sheet.next(); } } ]
            // the picture (photo, or the video's first frame until it plays)
            ImageView {
                visible: sheet.s.image !== "" && !(sheet.s.kind === "video" && player.mediaState === MediaState.Started)
                imageSource: sheet.s.image ? sheet.s.image : ""
                scalingMethod: ScalingMethod.AspectFit
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
            }
            ForeignWindowControl {
                id: video
                windowId: "gramStory"
                updatedProperties: WindowProperty.Position | WindowProperty.Size | WindowProperty.Visible
                visible: boundToWindow && sheet.s.kind === "video"
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
            }
            ActivityIndicator {
                running: sheet.shown && (sheet.s.loading || (sheet.s.kind === "photo" && sheet.s.image === "") || (sheet.s.kind === "video" && player.mediaState !== MediaState.Started))
                visible: running
                preferredWidth: ui.du(10); preferredHeight: ui.du(10)
                horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
            }
            Label {
                visible: sheet.s.error !== "" || sheet.s.kind === "other"
                horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                multiline: true
                text: sheet.s.error !== "" ? "This story isn't available any more." : "This kind of story can't be shown here."
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.White; textAlign: TextAlign.Center }
            }
            // top: progress bars, who, when, close
            Container {
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Top
                background: Color.create("#66000000")
                leftPadding: ui.du(1.5); rightPadding: ui.du(1); topPadding: ui.du(1.2)
                Container {
                    id: bars
                    horizontalAlignment: HorizontalAlignment.Fill
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    rightPadding: ui.du(0.5)
                    property int count: sheet.s.count ? sheet.s.count : 0
                    property int index: sheet.s.index ? sheet.s.index : 0
                    onCountChanged: bars.fill()
                    function fill() {
                        bars.removeAll();
                        for (var i = 0; i < bars.count; i++) { var b = barDef.createObject(); b.n = i; bars.add(b); }
                    }
                }
                Container {
                    topMargin: ui.du(1.2)
                    horizontalAlignment: HorizontalAlignment.Fill
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    Avatar { photo: sheet.s.photo ? sheet.s.photo : ""; initials: sheet.s.initials ? sheet.s.initials : ""; tint: sheet.s.color ? sheet.s.color : "#65aadd"
                             mask: "black"; size: ui.du(8); verticalAlignment: VerticalAlignment.Center }
                    Container {
                        leftMargin: ui.du(1.8)
                        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                        verticalAlignment: VerticalAlignment.Center
                        Label { text: sheet.s.title ? sheet.s.title : ""; textStyle { base: SystemDefaults.TextStyles.PrimaryText; color: Color.White; fontWeight: FontWeight.W500 } }
                        Label { text: sheet.s.time ? sheet.s.time : ""; topMargin: 0; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create("#ccffffff") } }
                    }
                    IconButton { tint: "#ffffffff"; icon: "asset:///icons/close.png"; verticalAlignment: VerticalAlignment.Center; onClicked: sheet.finish() }
                }
            }
            // bottom: caption, reply, heart
            Container {
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Bottom
                background: Color.create("#80000000")
                leftPadding: ui.du(2); rightPadding: ui.du(1); topPadding: ui.du(1.2); bottomPadding: ui.du(1.2)
                Label {
                    visible: sheet.s.caption && sheet.s.caption !== ""
                    text: sheet.s.caption ? sheet.s.caption : ""
                    multiline: true
                    textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.White }
                }
                Container {
                    visible: sheet.s.canReply === true
                    horizontalAlignment: HorizontalAlignment.Fill
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    TextField {
                        id: reply
                        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                        verticalAlignment: VerticalAlignment.Center
                        hintText: "Reply privately…"
                        input.submitKey: SubmitKey.Send
                        input.onSubmitted: { store.replyStory(reply.text); reply.text = ""; root.requestFocus(); }
                    }
                    IconButton {
                        verticalAlignment: VerticalAlignment.Center
                        visible: reply.text !== ""
                        tint: Skin.accent; icon: "asset:///icons/send.png"
                        onClicked: { store.replyStory(reply.text); reply.text = ""; root.requestFocus(); }
                    }
                    IconButton {
                        verticalAlignment: VerticalAlignment.Center
                        visible: reply.text === ""
                        tint: sheet.s.reaction === "❤" ? "#ffe0455a" : "#ffffffff"; icon: "asset:///icons/like.png"
                        onClicked: store.reactStory("❤")
                    }
                }
            }
        }
    }
    attachedObjects: [
        ComponentDefinition {
            id: barDef
            Container {
                id: bar
                property int n: 0
                property real part: n < bars.index ? 1 : n > bars.index ? 0 : Math.min(1, sheet.elapsed / Math.max(1, sheet.s.duration ? sheet.s.duration : 6))
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                leftMargin: ui.du(0.5)
                minHeight: ui.du(0.5); maxHeight: ui.du(0.5); preferredHeight: ui.du(0.5)
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                background: Color.create("#59ffffff")
                Container { background: Color.White; verticalAlignment: VerticalAlignment.Fill; layoutProperties: StackLayoutProperties { spaceQuota: bar.part * 1000 + 0.001 } }
                Container { verticalAlignment: VerticalAlignment.Fill; layoutProperties: StackLayoutProperties { spaceQuota: (1 - bar.part) * 1000 + 0.001 } }
            }
        },
        QTimer {
            id: tick
            interval: 100
            onTimeout: {
                if (!sheet.running()) return;
                if (sheet.s.kind === "video" && player.duration > 0) sheet.elapsed = player.position / 1000;
                else sheet.elapsed += 0.1;
                if (sheet.elapsed >= (sheet.s.duration ? sheet.s.duration : 6) || (sheet.s.error && sheet.s.error !== "" && sheet.elapsed >= 2)) sheet.next();
            }
        },
        QTimer { id: holdTimer; interval: 300; singleShot: true; onTimeout: sheet.held = true },
        MediaPlayer {
            id: player
            windowId: video.windowId
            windowGroupId: video.windowGroup
            videoOutput: VideoOutput.PrimaryDisplay
            onPlaybackCompleted: sheet.next()
        }
    ]
}
