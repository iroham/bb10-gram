// MessageRow: one message of the open chat. ListItemData = Store::messageRow().
//   kind: text | photo | video | animation | videonote | voice | audio | document | sticker | other
import bb.cascades 1.3
import gram 1.0

Container {
    id: row
    property bool out: false
    property variant st: row.ListItem.view ? row.ListItem.view.st : null     // the store (rows cannot see it directly)
    horizontalAlignment: HorizontalAlignment.Fill
    leftPadding: ui.du(1.5); rightPadding: ui.du(1.5); topPadding: ui.du(0.4); bottomPadding: ui.du(0.4)

    Container {                                    // the day line above the first message of a day
        visible: ListItemData.day !== ""
        horizontalAlignment: HorizontalAlignment.Center
        topMargin: ui.du(1.5); bottomMargin: ui.du(1.5)
        leftPadding: ui.du(2); rightPadding: ui.du(2); topPadding: ui.du(0.5); bottomPadding: ui.du(0.5)
        background: Color.create(Skin.dayChip)
        Label { text: ListItemData.day; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.text); fontWeight: FontWeight.W500 } }
    }


    Container {                                    // where the unread messages start
        visible: ListItemData.unreadLine === true
        horizontalAlignment: HorizontalAlignment.Fill
        topMargin: ui.du(1); bottomMargin: ui.du(1)
        topPadding: ui.du(0.6); bottomPadding: ui.du(0.6)
        background: Color.create(Skin.dayChip)
        Label { horizontalAlignment: HorizontalAlignment.Center; text: "Unread messages"
                textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.link); fontWeight: FontWeight.W500 } }
    }

    Container {                                    // the bubble
        id: bubble
        horizontalAlignment: row.out ? HorizontalAlignment.Right : HorizontalAlignment.Left
        maxWidth: ui.du(98)
        background: ListItemData.kind === "sticker" ? Color.Transparent : Color.create(row.out ? Skin.bubbleOut : Skin.bubbleIn)
        leftPadding: ui.du(1.6); rightPadding: ui.du(1.6); topPadding: ui.du(1); bottomPadding: ui.du(0.8)
        opacity: row.ListItem.active ? 0.7 : 1.0

        Label {
            visible: ListItemData.showSender
            text: ListItemData.sender
            textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.create(ListItemData.senderColor); fontWeight: FontWeight.W500 }
        }
        Label {
            visible: ListItemData.forward !== ""
            text: ListItemData.forward
            topMargin: 0
            textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.create(Skin.link); fontStyle: FontStyle.Italic }
        }
        Container {                                // what it answers
            visible: ListItemData.reply !== ""
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            topMargin: ui.du(0.4); bottomMargin: ui.du(0.6)
            Container { background: Color.create(Skin.accent); minWidth: ui.du(0.5); preferredWidth: ui.du(0.5); verticalAlignment: VerticalAlignment.Fill }
            Label {
                text: ListItemData.reply
                leftMargin: ui.du(1.2)
                textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.create(Skin.subtle) }
            }
        }
        Container {                                // photo, or the still picture of a video
            visible: ListItemData.imgW > 0
            layout: DockLayout {}
            topMargin: ui.du(0.4); bottomMargin: ui.du(0.6)
            preferredWidth: ListItemData.imgW; preferredHeight: ListItemData.imgH
            minWidth: ListItemData.imgW; minHeight: ListItemData.imgH
            background: ListItemData.kind === "sticker" ? Color.Transparent : Color.create(Skin.mediaBg)
            Pic {
                visible: !(anim.ready && ListItemData.anim !== "")
                src: ListItemData.image
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                scalingMethod: ScalingMethod.AspectFit
            }
            ImageView {                            // animated sticker
                visible: anim.ready && ListItemData.anim !== ""
                image: anim.image
                horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill
                scalingMethod: ScalingMethod.AspectFit
                attachedObjects: [ StickerPlayer { id: anim; size: 340; source: ListItemData.anim ? ListItemData.anim : "" } ]
            }
            ActivityIndicator {
                visible: ListItemData.image === "" && ListItemData.kind === "photo"
                running: visible
                horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                preferredWidth: ui.du(8); preferredHeight: ui.du(8)
            }
            ImageView {
                visible: ListItemData.kind === "video" || ListItemData.kind === "animation" || ListItemData.kind === "videonote"
                imageSource: ListItemData.fileState === "done" ? "asset:///icons/play.png" : ListItemData.fileState === "loading" ? "asset:///icons/sending.png" : "asset:///icons/download.png"
                horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                preferredWidth: ui.du(9); preferredHeight: ui.du(9)
            }
            Container {
                visible: ListItemData.duration !== "" && ListItemData.imgW > 0
                horizontalAlignment: HorizontalAlignment.Left; verticalAlignment: VerticalAlignment.Top
                background: Color.create("#aa000000")
                leftPadding: ui.du(0.8); rightPadding: ui.du(0.8)
                Label { text: ListItemData.duration + "  " + ListItemData.fileSize; textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.White } }
            }
        }
        Container {                                // a file, voice message or audio
            visible: ListItemData.fileId > 0 && ListItemData.imgW === 0
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            topMargin: ui.du(0.4); bottomMargin: ui.du(0.6)
            Container {
                layout: DockLayout {}
                minWidth: ui.du(9); minHeight: ui.du(9); preferredWidth: ui.du(9); preferredHeight: ui.du(9)
                verticalAlignment: VerticalAlignment.Center
                ImageView { imageSource: "asset:///icons/disc.png"; filterColor: Color.create(Skin.accent); horizontalAlignment: HorizontalAlignment.Fill; verticalAlignment: VerticalAlignment.Fill }
                ImageView {
                    imageSource: ListItemData.fileState === "loading" ? "asset:///icons/sending.png" : ListItemData.fileState === "none" ? "asset:///icons/download.png"
                               : (ListItemData.kind === "voice" || ListItemData.kind === "audio") ? "asset:///icons/play.png" : "asset:///icons/file.png"
                    horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center
                    preferredWidth: ui.du(5); preferredHeight: ui.du(5)
                }
            }
            Container {
                leftMargin: ui.du(1.6)
                verticalAlignment: VerticalAlignment.Center
                Label { text: ListItemData.fileName; textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text); fontWeight: FontWeight.W500 } }
                Label {
                    text: (ListItemData.duration !== "" ? ListItemData.duration + "  " : "") + ListItemData.fileSize
                    topMargin: 0
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.subtle) }
                }
            }
        }
        Container {                                // a call: direction, outcome, length; tap the handset to call back
            visible: ListItemData.kind === "call"
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            topMargin: ui.du(0.4); bottomMargin: ui.du(0.4)
            Container {
                verticalAlignment: VerticalAlignment.Center
                minWidth: ui.du(26)
                Label { text: ListItemData.callTitle ? ListItemData.callTitle : ""
                        textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text); fontWeight: FontWeight.W500 } }
                Container {
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    topMargin: 0
                    ImageView { imageSource: row.out ? "asset:///icons/callout.png" : "asset:///icons/callin.png"
                                filterColor: Color.create(ListItemData.callBad ? "#ffe0605a" : "#ff5fc56a")
                                preferredWidth: ui.du(2.6); preferredHeight: ui.du(2.6); verticalAlignment: VerticalAlignment.Center }
                    Label { text: ListItemData.time + (ListItemData.callSub ? ", " + ListItemData.callSub : "")
                            leftMargin: ui.du(0.6); verticalAlignment: VerticalAlignment.Center
                            textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.subtle) } }
                }
            }
            Container {
                layout: DockLayout {}
                leftMargin: ui.du(2)
                verticalAlignment: VerticalAlignment.Center
                minWidth: ui.du(8); minHeight: ui.du(8); preferredWidth: ui.du(8); preferredHeight: ui.du(8)
                ImageView { imageSource: "asset:///icons/call.png"; filterColor: Color.create(Skin.accent)
                            preferredWidth: ui.du(5); preferredHeight: ui.du(5)
                            horizontalAlignment: HorizontalAlignment.Center; verticalAlignment: VerticalAlignment.Center }
                gestureHandlers: [ TapHandler { onTapped: if (row.st && row.st.chat.canCall) row.st.calls.start(row.st.chat.callUser) } ]
            }
        }
        Label {                                    // the text (or caption)
            visible: ListItemData.hasText
            text: ListItemData.text
            multiline: true
            textFormat: TextFormat.Html
            content.flags: TextContentFlag.ActiveText | TextContentFlag.Emoticons
            textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text)
                        fontSize: ListItemData.kind === "sticker" ? FontSize.XXLarge : FontSize.Default }
        }
        Container {                                // link preview
            visible: ListItemData.hasLink === true
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            topMargin: ui.du(0.6); bottomMargin: ui.du(0.4)
            Container { background: Color.create(Skin.link); minWidth: ui.du(0.5); preferredWidth: ui.du(0.5); verticalAlignment: VerticalAlignment.Fill }
            Container {
                leftMargin: ui.du(1.2)
                Label { visible: ListItemData.linkSite ? true : false; text: ListItemData.linkSite ? ListItemData.linkSite : ""
                        textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.create(Skin.link); fontWeight: FontWeight.W500 } }
                Label { visible: ListItemData.linkTitle ? true : false; text: ListItemData.linkTitle ? ListItemData.linkTitle : ""; multiline: true; topMargin: 0
                        textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.create(Skin.text); fontWeight: FontWeight.W500 } }
                Label { visible: ListItemData.linkText ? true : false; text: ListItemData.linkText ? ListItemData.linkText : ""; multiline: true; topMargin: 0
                        textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.create(Skin.subtle) } }
            }
        }
        Container {                                // poll
            id: pollBox
            visible: ListItemData.pollQuestion ? true : false
            property variant opts: ListItemData.pollOpts
            property bool results: ListItemData.pollShowResults === true
            property variant msgId: ListItemData.id
            onOptsChanged: pollBox.fill()
            onResultsChanged: pollBox.fill()
            onCreationCompleted: pollBox.fill()
            function fill() {
                optBox.removeAll();
                if (!opts) return;
                for (var i = 0; i < opts.length; i++) {
                    var o = optDef.createObject();
                    o.index = i; o.msgId = msgId; o.text = opts[i].text; o.pct = opts[i].pct;
                    o.st = row.st; o.chosen = opts[i].chosen; o.picked = opts[i].picked; o.results = results;
                    optBox.add(o);
                }
            }
            Label { text: ListItemData.pollQuestion ? ListItemData.pollQuestion : ""; multiline: true
                    textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text); fontWeight: FontWeight.W500 } }
            Label { text: ListItemData.pollInfo ? ListItemData.pollInfo : ""; topMargin: 0
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.subtle) } }
            Container { id: optBox; topMargin: ui.du(0.8) }
            Label {
                visible: ListItemData.pollMulti === true && !pollBox.results
                text: ListItemData.pollPicked > 0 ? "Vote" : "Choose one or more"
                horizontalAlignment: HorizontalAlignment.Center; topMargin: ui.du(0.8)
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(ListItemData.pollPicked > 0 ? Skin.link : Skin.dim); fontWeight: FontWeight.W500 }
                gestureHandlers: [ TapHandler { onTapped: if (ListItemData.pollPicked > 0) row.st.sendVote(ListItemData.id) } ]
            }
            Label {
                visible: ListItemData.pollCanRetract === true
                text: "Retract vote"
                horizontalAlignment: HorizontalAlignment.Center; topMargin: ui.du(0.8)
                textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.link) }
                gestureHandlers: [ TapHandler { onTapped: row.st.retractVote(ListItemData.id) } ]
            }
        }
        Label {                                    // reactions
            visible: ListItemData.reactions !== ""
            text: ListItemData.reactions
            topMargin: ui.du(0.4)
            textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.create(Skin.text) }
        }
        Container {                                // time and state
            horizontalAlignment: HorizontalAlignment.Right
            layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
            topMargin: 0
            Label {
                text: (ListItemData.pinned ? "pinned  " : "") + (ListItemData.views !== "" ? ListItemData.views + " views  " : "") + (ListItemData.edited ? "edited " : "") + ListItemData.time
                verticalAlignment: VerticalAlignment.Center
                textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(row.out ? Skin.outMeta : Skin.dim); fontSize: FontSize.XSmall }
            }
            ImageView {
                visible: row.out && ListItemData.state !== ""
                imageSource: "asset:///icons/" + (ListItemData.state === "" ? "sent" : ListItemData.state) + ".png"
                filterColor: Color.create(ListItemData.state === "failed" ? Skin.error : Skin.outMeta)
                preferredWidth: ui.du(3.6); preferredHeight: ui.du(3.6); minWidth: ui.du(3.6)
                verticalAlignment: VerticalAlignment.Center
                leftMargin: ui.du(0.6)
            }
        }
    }

    Container {                                    // bot buttons under the bubble
        id: btnBox
        horizontalAlignment: row.out ? HorizontalAlignment.Right : HorizontalAlignment.Left
        maxWidth: ui.du(98)
        visible: ListItemData.buttons && ListItemData.buttons.length > 0
        property variant btns: ListItemData.buttons
        property variant msgId: ListItemData.id
        onBtnsChanged: btnBox.fill()
        onCreationCompleted: btnBox.fill()
        function fill() {
            btnBox.removeAll();
            if (!btns) return;
            var line = null, lastRow = -1;
            for (var i = 0; i < btns.length; i++) {
                if (btns[i].row !== lastRow) { line = lineDef.createObject(); btnBox.add(line); lastRow = btns[i].row; }
                var b = btnDef.createObject();
                b.index = i; b.msgId = msgId; b.text = btns[i].text; b.st = row.st;
                line.add(b);
            }
        }
    }

    attachedObjects: [
        ComponentDefinition {
            id: optDef
            Container {
                id: opt
                property variant st
                property int index: 0
                property variant msgId: 0
                property string text: ""
                property int pct: 0
                property bool chosen: false
                property bool picked: false
                property bool results: false
                property bool down: false
                horizontalAlignment: HorizontalAlignment.Fill
                topPadding: ui.du(0.7); bottomPadding: ui.du(0.7)
                background: opt.down ? Color.create(Skin.press) : Color.Transparent
                onTouch: { if (event.isDown()) opt.down = true; else if (event.isUp() || event.isCancel()) opt.down = false; }
                gestureHandlers: [ TapHandler { onTapped: if (!opt.results) opt.st.votePoll(opt.msgId, opt.index) } ]
                Container {
                    layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                    Label {
                        text: opt.results ? opt.pct + "%" : (opt.picked ? "\u25C9" : "\u25CB")
                        minWidth: ui.du(7)
                        textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(opt.picked || opt.chosen ? Skin.link : "#ffb8c7d6"); fontWeight: FontWeight.W500 }
                    }
                    Label {
                        text: opt.text + (opt.chosen ? "  \u2713" : ""); multiline: true
                        layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                        textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.text) }
                    }
                }
                Container {
                    visible: opt.results
                    topMargin: ui.du(0.4); leftMargin: ui.du(7)
                    background: Color.create(opt.chosen ? Skin.link : Skin.pollTrack)
                    minHeight: ui.du(0.6); preferredHeight: ui.du(0.6)
                    preferredWidth: Math.max(ui.du(1), ui.du(70) * opt.pct / 100)
                }
            }
        },
        ComponentDefinition {
            id: lineDef
            Container {
                layout: StackLayout { orientation: LayoutOrientation.LeftToRight }
                horizontalAlignment: HorizontalAlignment.Fill; topMargin: ui.du(0.5)
            }
        },
        ComponentDefinition {
            id: btnDef
            Container {
                id: bb
                property variant st
                property int index: 0
                property variant msgId: 0
                property string text: ""
                property bool down: false
                layoutProperties: StackLayoutProperties { spaceQuota: 1 }
                leftMargin: ui.du(0.5)
                topPadding: ui.du(1.2); bottomPadding: ui.du(1.2); leftPadding: ui.du(1); rightPadding: ui.du(1)
                background: Color.create(bb.down ? Skin.keyDown : Skin.keyBg)
                onTouch: { if (event.isDown()) bb.down = true; else if (event.isUp() || event.isCancel()) bb.down = false; }
                gestureHandlers: [ TapHandler { onTapped: bb.st.pressButton(bb.msgId, bb.index) } ]
                Label { text: bb.text; horizontalAlignment: HorizontalAlignment.Center
                        textStyle { base: SystemDefaults.TextStyles.SubtitleText; color: Color.create(Skin.text); fontWeight: FontWeight.W500 } }
            }
        }
    ]

    contextActions: [
        ActionSet {
            title: "Message"
            ActionItem { title: "Reply"; imageSource: "asset:///icons/reply.png"; onTriggered: row.ListItem.view.act("reply", row.ListItem.indexPath) }
            ActionItem { title: "React"; imageSource: "asset:///icons/like.png"; onTriggered: row.ListItem.view.act("react", row.ListItem.indexPath) }
            ActionItem { title: "Copy text"; imageSource: "asset:///icons/copy.png"; enabled: ListItemData.hasText; onTriggered: row.ListItem.view.act("copy", row.ListItem.indexPath) }
            ActionItem { title: "Forward"; imageSource: "asset:///icons/forward.png"; onTriggered: row.ListItem.view.act("forward", row.ListItem.indexPath) }
            ActionItem { title: ListItemData.pinned ? "Unpin" : "Pin"; imageSource: "asset:///icons/pin.png"; onTriggered: row.ListItem.view.act("pin", row.ListItem.indexPath) }
            ActionItem { title: "Edit"; imageSource: "asset:///icons/edit.png"; enabled: row.out && ListItemData.kind === "text"; onTriggered: row.ListItem.view.act("edit", row.ListItem.indexPath) }
            ActionItem { title: "Delete for everyone"; imageSource: "asset:///icons/delete.png"; enabled: row.out; onTriggered: row.ListItem.view.act("deleteAll", row.ListItem.indexPath) }
            DeleteActionItem { title: "Delete for me"; onTriggered: row.ListItem.view.act("delete", row.ListItem.indexPath) }
        }
    ]
}
