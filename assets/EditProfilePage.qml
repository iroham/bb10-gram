// EditProfilePage: the account's first and last name, bio and username. Saves only what changed.
import bb.cascades 1.3
import gram 1.0

Page {
    id: page
    property variant me: store.me
    property bool filled: false
    signal goBack()
    actionBarVisibility: ChromeVisibility.Hidden
    onMeChanged: page.fill()
    onCreationCompleted: { store.loadMyInfo(); page.fill(); }
    function fill() {
        if (!page.me || !page.me.id) return;
        if (!page.filled) { first.text = page.me.first ? page.me.first : ""; last.text = page.me.last ? page.me.last : "";
                            user.text = page.me.username ? page.me.username.substring(1) : ""; page.filled = true; }
        if (bio.text === "" && page.me.bio) bio.text = page.me.bio;
    }
    function save() { store.saveProfile(first.text, last.text, bio.text, user.text); page.goBack(); }
    Container {
        background: Color.create(Skin.listBg)
        TopBar { back: true; title: "Edit profile"; secondIcon: "done"; onSecondClicked: page.save(); onBackClicked: page.goBack() }
        ScrollView {
            layoutProperties: StackLayoutProperties { spaceQuota: 1 }
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                leftPadding: ui.du(3); rightPadding: ui.du(3); topPadding: ui.du(2); bottomPadding: ui.du(4)
                Section { text: "Your name" }
                TextField { id: first; hintText: "First name (required)"; input.submitKey: SubmitKey.Next }
                TextField { id: last; hintText: "Last name (optional)"; topMargin: ui.du(1.5) }
                Section { text: "Bio" }
                TextArea { id: bio; hintText: "A few words about you"; maxHeight: ui.du(20) }
                Label { text: (70 - bio.text.length) + " characters left"; horizontalAlignment: HorizontalAlignment.Right
                        textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(bio.text.length > 70 ? Skin.error : Skin.dim) } }
                Section { text: "Username" }
                TextField { id: user; hintText: "username"; inputMode: TextFieldInputMode.Url }
                Label {
                    multiline: true
                    text: user.text === "" ? "Without a username, people find you only by your phone number." : "People can find you as @" + user.text + " and open t.me/" + user.text + ". At least 5 characters: a–z, 0–9 and underscores."
                    textStyle { base: SystemDefaults.TextStyles.SmallText; color: Color.create(Skin.dim) }
                }
            }
        }
    }
}
