// StoryBar: the row of people and channels with active stories above the chat list. A coloured ring = not seen yet.
import bb.cascades 1.3
import gram 1.0

ScrollView {
    id: bar
    signal opened(variant chatId)
    horizontalAlignment: HorizontalAlignment.Fill
    scrollViewProperties.scrollMode: ScrollMode.Horizontal
    property variant list: store.stories
    onListChanged: row.fill()
    onCreationCompleted: row.fill()
    Container {
        id: row
        layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
        leftPadding: ui.du(1.5); rightPadding: ui.du(1.5); topPadding: ui.du(1.5); bottomPadding: ui.du(1)
        function fill() {
            row.removeAll();
            var l = bar.list;
            for (var i = 0; l && i < l.length; i++) {
                var s = itemDef.createObject();
                s.chatId = l[i].chatId; s.title = l[i].title; s.unread = l[i].unread;
                s.photo = l[i].photo; s.initials = l[i].initials; s.tint = l[i].color;
                row.add(s);
            }
        }
    }
    attachedObjects: [
        ComponentDefinition {
            id: itemDef
            Container {
                id: it
                property variant chatId: 0
                property string title: ""
                property bool unread: true
                property alias photo: av.photo
                property alias initials: av.initials
                property alias tint: av.tint
                leftMargin: ui.du(1); rightMargin: ui.du(1)
                preferredWidth: ui.du(14)
                gestureHandlers: [ TapHandler { onTapped: bar.opened(it.chatId) } ]
                Container {
                    horizontalAlignment: HorizontalAlignment.Center
                    layout: DockLayout {}
                    ImageView { imageSource: "asset:///icons/disc.png"; filterColor: Color.create(it.unread ? Skin.accent : Skin.line)
                                preferredWidth: ui.du(12.4); preferredHeight: ui.du(12.4) }
                    ImageView { imageSource: "asset:///icons/disc.png"; filterColor: Color.create(Skin.listBg)
                                preferredWidth: ui.du(11.6); preferredHeight: ui.du(11.6)
                                horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center }
                    Avatar { id: av; size: ui.du(10.6); horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center }
                }
                Label {
                    horizontalAlignment: HorizontalAlignment.Center
                    topMargin: ui.du(0.6)
                    text: it.title
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(it.unread ? Skin.text : Skin.dim); textAlign: TextAlign.Center }
                }
            }
        }
    ]
}
