// CoverView: what the minimized tile on the BB10 home screen shows: unread total and the three latest chats
// (like Device Monitor's or File Manager's tiles), instead of a shrunken copy of the window.
import bb.cascades 1.3
import gram 1.0

Container {
    id: cv
    property variant c: store.cover
    background: Color.create(Skin.listBg)
    horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
    Container {                                    // a call going on: green header with who and how long
        visible: store.calls.active
        horizontalAlignment: HorizontalAlignment.Fill
        background: Color.create(store.calls.phase === "ended" ? "#ffb04a45" : "#ff3a9d4a")
        leftPadding: 18; rightPadding: 18; topPadding: 12; bottomPadding: 12
        Label {
            text: appLock.enabled ? "Call in progress" : store.calls.name
            textStyle { fontSize: FontSize.PointValue; fontSizeValue: 8; fontWeight: FontWeight.W500; color: Color.White }
        }
        Label {
            topMargin: 0
            text: store.calls.phase === "talking" ? (store.calls.muted ? "Muted · " : "In call · ") + nav.clock(store.calls.seconds) : store.calls.status
            textStyle { fontSize: FontSize.PointValue; fontSizeValue: 6; color: Color.White }
        }
    }
    Container {                                    // header: the unread line
        visible: !store.calls.active
        horizontalAlignment: HorizontalAlignment.Fill
        background: Color.create(Skin.headerBg)
        leftPadding: 18; rightPadding: 18; topPadding: 12; bottomPadding: 12
        Label {
            text: appLock.enabled ? "Gram" : cv.c.line ? cv.c.line : "Gram"
            multiline: true
            textStyle { fontSize: FontSize.PointValue; fontSizeValue: 8; fontWeight: FontWeight.W500
                        color: Color.create(Skin.headerText) }
        }
    }
    Container { horizontalAlignment: HorizontalAlignment.Fill; preferredHeight: 3; minHeight: 3; background: Color.create(Skin.accent) }
    Container {                                    // app lock on: no chats, names or text on the tile
        visible: appLock.enabled
        horizontalAlignment: HorizontalAlignment.Fill
        topPadding: 40
        ImageView { horizontalAlignment: HorizontalAlignment.Center; imageSource: "asset:///icons/lock.png"; filterColor: Color.create(Skin.dim); preferredWidth: 80; preferredHeight: 80 }
        Label {
            horizontalAlignment: HorizontalAlignment.Center
            text: "Locked"
            textStyle { fontSize: FontSize.PointValue; fontSizeValue: 7; color: Color.create(Skin.dim) }
        }
    }
    Container {
        id: rowsBox
        visible: !appLock.enabled
        horizontalAlignment: HorizontalAlignment.Fill
        leftPadding: 18; rightPadding: 18
        property variant rows: cv.c.rows
        onRowsChanged: rowsBox.fill()
        onCreationCompleted: rowsBox.fill()
        function fill() {
            rowsBox.removeAll();
            if (!rows) return;
            for (var i = 0; i < rows.length; i++) {
                var r = rowDef.createObject();
                r.title = rows[i].title; r.text = rows[i].text; r.unread = rows[i].unread; r.muted = rows[i].muted;
                r.last = i === rows.length - 1;
                rowsBox.add(r);
            }
        }
    }
    attachedObjects: [
        ComponentDefinition {
            id: rowDef
            Container {
                id: row
                property string title: ""
                property string text: ""
                property int unread: 0
                property bool muted: false
                property bool last: false
                horizontalAlignment: HorizontalAlignment.Fill
                topPadding: 12; bottomPadding: 12
                Container {
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    horizontalAlignment: HorizontalAlignment.Fill
                    Label {
                        text: row.title
                        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                        textStyle { fontSize: FontSize.PointValue; fontSizeValue: 7; fontWeight: FontWeight.W500; color: Color.create(Skin.text) }
                    }
                    Container {
                        visible: row.unread > 0
                        background: Color.create(row.muted ? Skin.badgeMuted : Skin.badge)
                        leftPadding: 10; rightPadding: 10
                        verticalAlignment: VerticalAlignment.Center
                        Label { text: row.unread > 99 ? "99+" : "" + row.unread
                                textStyle { fontSize: FontSize.PointValue; fontSizeValue: 6; fontWeight: FontWeight.W500; color: Color.White } }
                    }
                }
                Label {
                    text: row.text; topMargin: 0
                    textStyle { fontSize: FontSize.PointValue; fontSizeValue: 6; color: Color.create(Skin.dim) }
                }
                Container { visible: !row.last; topMargin: 12; horizontalAlignment: HorizontalAlignment.Fill; preferredHeight: 1; minHeight: 1; background: Color.create(Skin.line) }
            }
        }
    ]
}
