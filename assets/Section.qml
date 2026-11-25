// Section: a small coloured heading between groups of settings.
import bb.cascades 1.3
import gram 1.0

Container {
    id: sec
    property string text: ""
    horizontalAlignment: HorizontalAlignment.Fill
    leftPadding: ui.du(3); topPadding: ui.du(3); bottomPadding: ui.du(0.5)
    Label { text: sec.text; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.accent); fontWeight: FontWeight.W500 } }
}
