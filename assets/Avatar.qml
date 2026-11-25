// Avatar: round picture of a chat or person; without a photo, initials on a coloured disc.
// The photo is square; a ring in the page's background colour (mask) rounds it off.
import bb.cascades 1.3
import gram 1.0

Container {
    id: av
    property string photo: ""
    property string initials: ""
    property string tint: "#65aadd"
    property real size: ui.du(12)
    property string mask: "list"          // which background it sits on: list | chat | header
    property bool big: false              // large initials (call screen)
    layout: DockLayout {}
    preferredWidth: size; preferredHeight: size; minWidth: size; minHeight: size; maxWidth: size; maxHeight: size
    ImageView {
        visible: av.photo === ""
        imageSource: "asset:///icons/disc.png"
        filterColor: Color.create(av.tint)
        horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
    }
    Label {
        visible: av.photo === ""
        text: av.initials
        horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
        textStyle { base: SystemDefaults.TextStyles.TitleText; color: Color.White; fontWeight: FontWeight.W500
                    fontSize: av.big ? FontSize.PointValue : FontSize.Default; fontSizeValue: 24 }
    }
    Pic {
        visible: av.photo !== ""
        src: av.photo
        horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
    }
    ImageView {
        visible: av.photo !== ""
        imageSource: "asset:///icons/mask.png"
        filterColor: Color.create(av.mask === "black" ? "#ff000000" : av.mask === "header" ? Skin.headerBg : av.mask === "chat" ? Skin.chatBg : av.mask === "panel" ? Skin.panelBg : Skin.listBg)
        horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
    }
}
