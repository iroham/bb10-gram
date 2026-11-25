// TopBar: the bar at the top of a page: optional back button, optional avatar, title + small line, buttons.
import bb.cascades 1.3
import gram 1.0

Container {
    id: bar
    property string title: ""
    property string subtitle: ""
    property bool subtitleAccent: false
    property bool back: false
    property bool avatar: false
    property string photo: ""
    property string initials: ""
    property string tint: "#65aadd"
    property string firstIcon: ""         // icon names of up to two buttons on the right
    property string secondIcon: ""
    signal firstClicked()
    signal secondClicked()
    signal backClicked()
    signal titleClicked()
    horizontalAlignment: HorizontalAlignment.Fill
    background: Color.create(Skin.headerBg)
    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
    minHeight: ui.du(11); leftPadding: bar.back ? 0 : ui.du(3); rightPadding: ui.du(1)
    IconButton { tint: Skin.headerText; visible: bar.back; icon: "asset:///icons/back.png"; verticalAlignment: VerticalAlignment.Center; onClicked: bar.backClicked() }
    Avatar {
        visible: bar.avatar
        photo: bar.photo; initials: bar.initials; tint: bar.tint; mask: "header"; size: ui.du(8.4)
        verticalAlignment: VerticalAlignment.Center
        rightMargin: ui.du(2)
    }
    Container {
        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
        verticalAlignment: VerticalAlignment.Center
        gestureHandlers: [ TapHandler { onTapped: bar.titleClicked() } ]
        Label {
            text: bar.title
            textStyle { base: SystemDefaults.TextStyles.TitleText; color: Color.create(Skin.headerText); fontWeight: FontWeight.W500 }
        }
        Label {
            visible: bar.subtitle !== ""
            text: bar.subtitle
            topMargin: 0
            textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(bar.subtitleAccent ? (Skin.dark ? Skin.accent : Skin.headerText) : Skin.headerDim) }
        }
    }
    IconButton { tint: Skin.headerText; visible: bar.firstIcon !== ""; icon: "asset:///icons/" + (bar.firstIcon === "" ? "more" : bar.firstIcon) + ".png"; verticalAlignment: VerticalAlignment.Center; onClicked: bar.firstClicked() }
    IconButton { tint: Skin.headerText; visible: bar.secondIcon !== ""; icon: "asset:///icons/" + (bar.secondIcon === "" ? "more" : bar.secondIcon) + ".png"; verticalAlignment: VerticalAlignment.Center; onClicked: bar.secondClicked() }
}
