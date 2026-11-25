// SwitchListPage: a list of on/off rights with Save in the top bar. Used for an admin's rights, a member's
// restrictions (with "for how long") and a group's default permissions (with slow mode).
//   items: [{key, title, sub, on}]   extra: "" | "duration" | "slowmode"
import bb.cascades 1.3
import bb.system 1.2
import gram 1.0

Page {
    id: page
    signal goBack()
    signal saved(variant picked, int seconds)
    property string title: ""
    property string note: ""
    property variant items: []
    property string extra: ""
    property int seconds: 0                  // duration (restrict) or slow mode delay
    property variant picked: ({})
    property string danger: ""               // optional red action at the bottom (e.g. "Dismiss admin")
    signal dangerClicked()
    actionBarVisibility: ChromeVisibility.Hidden
    onItemsChanged: {
        var p = {};
        for (var i = 0; i < items.length; i++) p[items[i].key] = items[i].on === true;
        page.picked = p;
        rows.fill();
    }
    function set(key, on) { var p = page.picked; p[key] = on; page.picked = p; }
    property variant durations: [[0, "Forever"], [3600, "For 1 hour"], [86400, "For 1 day"], [604800, "For 1 week"]]
    property variant slowModes: [[0, "Off"], [10, "10 seconds"], [30, "30 seconds"], [60, "1 minute"], [300, "5 minutes"], [900, "15 minutes"], [3600, "1 hour"]]
    function nameOf(list, v) { for (var i = 0; i < list.length; i++) if (list[i][0] === v) return list[i][1]; return list[0][1]; }
    function askExtra() {
        var list = page.extra === "duration" ? page.durations : page.slowModes;
        extraDlg.title = page.extra === "duration" ? "Restrict" : "Slow mode";
        extraDlg.body = page.extra === "duration" ? "" : "Members can send one message per chosen time.";
        extraDlg.clearList();
        for (var i = 0; i < list.length; i++) extraDlg.appendItem(list[i][1], true, list[i][0] === page.seconds);
        extraDlg.show();
    }
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: page.title; firstIcon: "done"; onBackClicked: page.goBack(); onFirstClicked: { page.saved(page.picked, page.seconds); page.goBack(); } }
        ScrollView {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                bottomPadding: ui.du(4)
                Label {
                    visible: page.note !== ""
                    multiline: true
                    leftMargin: ui.du(3); rightMargin: ui.du(3); topMargin: ui.du(2)
                    text: page.note
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) }
                }
                Section { text: page.extra === "duration" ? "What they can do" : page.extra === "slowmode" ? "What members can do" : "Rights" }
                Container {
                    id: rows
                    horizontalAlignment: HorizontalAlignment.Fill
                    function fill() {
                        rows.removeAll();
                        for (var i = 0; i < page.items.length; i++) {
                            var r = rowDef.createObject();
                            r.key = page.items[i].key; r.title = page.items[i].title; r.sub = page.items[i].sub ? page.items[i].sub : "";
                            r.checked = page.items[i].on === true;
                            rows.add(r);
                        }
                    }
                }
                Container {
                    visible: page.extra !== ""
                    horizontalAlignment: HorizontalAlignment.Fill
                    Section { text: page.extra === "duration" ? "Duration" : "Slow mode" }
                    OptionRow { icon: "settings"; title: page.nameOf(page.extra === "duration" ? page.durations : page.slowModes, page.seconds); onClicked: page.askExtra() }
                }
                Container {
                    visible: page.danger !== ""
                    horizontalAlignment: HorizontalAlignment.Fill
                    topMargin: ui.du(2)
                    OptionRow { icon: "close"; title: page.danger; danger: true; onClicked: { page.dangerClicked(); page.goBack(); } }
                }
            }
        }
    }
    attachedObjects: [
        ComponentDefinition {
            id: rowDef
            OptionRow {
                property string key: ""
                hasSwitch: true
                onToggled: page.set(key, on)
            }
        },
        SystemListDialog {
            id: extraDlg
            selectionMode: ListSelectionMode.Single
            confirmButton.label: "OK"
            cancelButton.label: "Cancel"
            onFinished: {
                if (value !== SystemUiResult.ConfirmButtonSelection || !selectedIndices.length) return;
                var list = page.extra === "duration" ? page.durations : page.slowModes;
                page.seconds = list[selectedIndices[0]][0];
            }
        }
    ]
}
