// PhotoPage: one photo, full screen; pinch to zoom. Drag the photo down (or up) to close it: it follows the finger
// and the black backdrop fades; let go past a quarter of the screen and it flies out and the chat comes back,
// otherwise it springs back. Disabled while zoomed in (then dragging pans the photo).
import bb.cascades 1.3

Page {
    id: page
    property string path: ""
    property real startY: 0
    property real dy: 0
    property bool dragging: false
    property bool closing: false
    property real h: 1440
    signal goBack()
    actionBarVisibility: ChromeVisibility.Hidden

    function release() {
        if (!page.dragging) return;
        page.dragging = false;
        if (Math.abs(page.dy) > page.h * 0.25) {
            page.closing = true;
            out.toY = page.dy > 0 ? page.h : -page.h;
            out.play();
        } else back.play();
    }
    Container {
        id: root
        layout: DockLayout {}
        background: Color.create("#ff0e1621")
        attachedObjects: [ LayoutUpdateHandler { onLayoutFrameChanged: if (layoutFrame.height > 0) page.h = layoutFrame.height } ]
        Container {                               // black backdrop; fades as the photo is dragged away
            horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
            background: Color.Black
            opacity: Math.max(0, 1 - Math.abs(photo.translationY) / (page.h * 0.6))
        }
        Container {
            id: photo
            horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
            attachedObjects: [ ImplicitAnimationController { propertyName: "translationY"; enabled: false } ]
            ScrollView {
                id: zoom
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                scrollViewProperties { scrollMode: ScrollMode.Both; pinchToZoomEnabled: true; minContentScale: 1.0; maxContentScale: 6.0 }
                ImageView {
                    imageSource: page.path
                    scalingMethod: ScalingMethod.AspectFit
                    preferredWidth: 1440; preferredHeight: 1440
                }
            }
            animations: [
                TranslateTransition { id: back; toY: 0; duration: 220; easingCurve: StockCurve.CubicOut; onEnded: page.dy = 0 },
                TranslateTransition { id: out; duration: 180; easingCurve: StockCurve.CubicIn; onEnded: page.goBack() }
            ]
        }
        IconButton { tint: "#ffffffff"; icon: "asset:///icons/back.png"; horizontalAlignment: HorizontalAlignment.Left; verticalAlignment: VerticalAlignment.Top
                     opacity: page.dragging || page.closing ? 0 : 1; onClicked: page.goBack() }
        onTouchCapture: {
            if (page.closing) return;
            if (event.isDown()) { page.startY = event.windowY; page.dy = 0; page.dragging = false; }
            else if (event.isMove()) {
                var d = event.windowY - page.startY;
                if (!page.dragging && zoom.contentScale <= 1.01 && Math.abs(d) > 24) page.dragging = true;
                if (page.dragging) { page.dy = d; photo.translationY = d; }
            } else if (event.isUp() || event.isCancel()) page.release();
        }
    }
}
