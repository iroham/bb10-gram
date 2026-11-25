// OptionRow: a settings line: icon, title, small line, optional switch; tap = clicked (or toggles the switch).
import bb.cascades 1.3
import gram 1.0

Container {
    id: row
    property string icon: ""
    property string title: ""
    property string sub: ""
    property bool hasSwitch: false
    property bool checked: false
    property bool danger: false
    property bool down: false
    signal clicked()
    signal toggled(bool on)
    function sync() { sw.checked = row.checked; }       // put the switch back to the value (a cancelled change)
    onCheckedChanged: sw.checked = row.checked
    horizontalAlignment: HorizontalAlignment.Fill
    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
    background: row.down ? Color.create(Skin.press) : Color.Transparent
    leftPadding: ui.du(3); rightPadding: ui.du(2.5); topPadding: ui.du(1.8); bottomPadding: ui.du(1.8)
    onTouch: { if (event.isDown()) row.down = true; else if (event.isUp() || event.isCancel()) row.down = false; }
    gestureHandlers: [ TapHandler { onTapped: { if (row.hasSwitch) { sw.checked = !sw.checked; } else row.clicked(); } } ]
    ImageView {
        visible: row.icon !== ""
        imageSource: row.icon === "" ? "" : "asset:///icons/" + row.icon + ".png"
        filterColor: Color.create(row.danger ? Skin.error : Skin.dim)
        preferredWidth: ui.du(5.5); preferredHeight: ui.du(5.5); minWidth: ui.du(5.5)
        verticalAlignment: VerticalAlignment.Center
        rightMargin: ui.du(3)
    }
    Container {
        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
        verticalAlignment: VerticalAlignment.Center
        Label { text: row.title; multiline: true; textStyle { base: SystemDefaults.TextStyles.PrimaryText; color: Color.create(row.danger ? Skin.error : Skin.text) } }
        Label { visible: row.sub !== ""; text: row.sub; multiline: true; topMargin: 0; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) } }
    }
    ToggleButton {
        id: sw
        visible: row.hasSwitch
        checked: row.checked
        verticalAlignment: VerticalAlignment.Center
        onCheckedChanged: if (checked !== row.checked) row.toggled(checked)
    }
}
