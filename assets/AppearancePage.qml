// AppearancePage (Settings > Appearance): Dark or Light, the accent colour, with a live preview of a chat.
import bb.cascades 1.3
import gram 1.0

Page {
    id: page
    signal goBack()
    actionBarVisibility: ChromeVisibility.Hidden
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: "Appearance"; onBackClicked: page.goBack() }
        ScrollView {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                bottomPadding: ui.du(4)
                Container {                       // preview: two bubbles on the chat background
                    horizontalAlignment: HorizontalAlignment.Fill
                    background: Color.create(Skin.chatBg)
                    topPadding: ui.du(2.5); bottomPadding: ui.du(2.5); leftPadding: ui.du(2.5); rightPadding: ui.du(2.5)
                    Container {
                        horizontalAlignment: HorizontalAlignment.Left
                        background: Color.create(Skin.bubbleIn)
                        leftPadding: ui.du(2); rightPadding: ui.du(2); topPadding: ui.du(1.2); bottomPadding: ui.du(1.2)
                        Label { text: "Did you see the new colours?"; textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text) } }
                        Label { horizontalAlignment: HorizontalAlignment.Right; text: "9:41"; topMargin: 0; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) } }
                    }
                    Container {
                        topMargin: ui.du(1.5)
                        horizontalAlignment: HorizontalAlignment.Right
                        background: Color.create(Skin.bubbleOut)
                        leftPadding: ui.du(2); rightPadding: ui.du(2); topPadding: ui.du(1.2); bottomPadding: ui.du(1.2)
                        Label { text: "Yes, I'm trying them now"; textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text) } }
                        Label { horizontalAlignment: HorizontalAlignment.Right; text: "9:42 ✓✓"; topMargin: 0; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.outMeta) } }
                    }
                }
                Section { text: "Theme" }
                OptionRow { icon: skin.mode === "dark" ? "done" : ""; title: "Dark"; sub: "Easy on the eyes at night"; onClicked: skin.setMode("dark") }
                OptionRow { icon: skin.mode === "light" ? "done" : ""; title: "Light"; sub: "Bright and clear in daylight"; onClicked: skin.setMode("light") }
                Section { text: "Accent colour" }
                Container {
                    id: swatches
                    horizontalAlignment: HorizontalAlignment.Fill
                    leftPadding: ui.du(2); topPadding: ui.du(1.5)
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    property variant list: skin.accents
                    onListChanged: fill()
                    onCreationCompleted: fill()
                    function fill() {
                        swatches.removeAll();
                        for (var i = 0; i < list.length; i++) {
                            var s = swatchDef.createObject();
                            s.name = list[i].name; s.color = list[i].color;
                            swatches.add(s);
                        }
                    }
                }
                Label {
                    leftMargin: ui.du(3); rightMargin: ui.du(3); topMargin: ui.du(1)
                    text: skin.accentName
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) }
                }
            }
        }
    }
    attachedObjects: [
        ComponentDefinition {
            id: swatchDef
            Container {
                id: sw
                property string name: ""
                property string color: "#ff5aa7e6"
                leftMargin: ui.du(1); rightMargin: ui.du(1)
                layout: DockLayout {}
                gestureHandlers: [ TapHandler { onTapped: skin.setAccent(sw.name) } ]
                ImageView { imageSource: "asset:///icons/disc.png"; filterColor: Color.create(sw.color); preferredWidth: ui.du(11); preferredHeight: ui.du(11) }
                ImageView {
                    visible: skin.accentName === sw.name
                    imageSource: "asset:///icons/done.png"; filterColor: Color.White
                    preferredWidth: ui.du(6); preferredHeight: ui.du(6)
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                }
            }
        }
    ]
}
