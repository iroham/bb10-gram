// SignIn: the steps before the chat list: API credentials (once), phone number, code, two-step password.
import bb.cascades 1.3
import gram 1.0

ScrollView {
    id: si
    property string step: store.authState
    property bool busy: false
    property int wait: 0                       // seconds until the code may be sent again
    property variant info: store.authInfo
    onStepChanged: si.busy = false
    onInfoChanged: { si.busy = false; si.wait = info.timeout ? info.timeout : 0; if (si.wait > 0) tick.start(); }
    attachedObjects: [ QTimer { id: tick; interval: 1000; onTimeout: { si.wait--; if (si.wait <= 0) tick.stop(); } } ]
    horizontalAlignment: HorizontalAlignment.Fill
    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        leftPadding: ui.du(5); rightPadding: ui.du(5); topPadding: ui.du(5); bottomPadding: ui.du(5)
        attachedObjects: [ TextStyleDefinition { id: body; base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.subtle) } ]

        ActivityIndicator {
            visible: si.step === "starting" || si.busy
            running: visible
            horizontalAlignment: HorizontalAlignment.Center
            preferredWidth: ui.du(10); preferredHeight: ui.du(10)
        }

        // --- API credentials
        Container {
            visible: si.step === "credentials"
            Label { text: "Set up"; textStyle { base: SystemDefaults.TextStyles.BigText; color: Color.create(Skin.text) } }
            Label {
                multiline: true
                text: "Telegram requires every app to use its own API ID and API hash. Create yours once: open my.telegram.org in a browser, sign in with your phone number, choose \"API development tools\" and fill in the short form. They stay on this phone only."
                textStyle.base: body.style
            }
            TextField { id: apiId; hintText: "API ID (a number)"; inputMode: TextFieldInputMode.NumbersAndPunctuation; topMargin: ui.du(3) }
            TextField { id: apiHash; hintText: "API hash (32 letters and digits)"; input { flags: TextInputFlag.AutoCapitalizationOff | TextInputFlag.AutoCorrectionOff | TextInputFlag.SpellCheckOff } }
            Button { text: "Continue"; topMargin: ui.du(3); horizontalAlignment: HorizontalAlignment.Fill; onClicked: { si.busy = true; store.setCredentials(apiId.text, apiHash.text); } }
        }
        // --- phone number
        Container {
            visible: si.step === "phone"
            Label { text: "Your phone number"; textStyle { base: SystemDefaults.TextStyles.BigText; color: Color.create(Skin.text) } }
            Label { multiline: true; text: "Enter the number of your Telegram account with the country code. Telegram then sends a code."; textStyle.base: body.style }
            TextField {
                id: phone; hintText: "+49 170 1234567"; inputMode: TextFieldInputMode.PhoneNumber; topMargin: ui.du(3)
                input { submitKey: SubmitKey.Next; onSubmitted: { si.busy = true; store.setPhone(phone.text); } }
            }
            Button { text: "Next"; topMargin: ui.du(3); horizontalAlignment: HorizontalAlignment.Fill; onClicked: { si.busy = true; store.setPhone(phone.text); } }
            Label {
                text: "Change API ID and hash"
                topMargin: ui.du(5)
                horizontalAlignment: HorizontalAlignment.Center
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.link) }
                gestureHandlers: [ TapHandler { onTapped: store.resetCredentials() } ]
            }
        }
        // --- code
        Container {
            visible: si.step === "code"
            Label { text: "Enter the code"; textStyle { base: SystemDefaults.TextStyles.BigText; color: Color.create(Skin.text) } }
            Label { multiline: true; text: (store.authInfo.where ? store.authInfo.where : "") + (store.authInfo.phone ? " (+" + store.authInfo.phone + ")" : ""); textStyle.base: body.style }
            TextField {
                id: code; hintText: "Code"; inputMode: TextFieldInputMode.NumbersAndPunctuation; topMargin: ui.du(3)
                input { submitKey: SubmitKey.Next; onSubmitted: { si.busy = true; store.checkCode(code.text); } }
            }
            Button { text: "Next"; topMargin: ui.du(3); horizontalAlignment: HorizontalAlignment.Fill; onClicked: { si.busy = true; store.checkCode(code.text); } }
            Label {
                visible: store.authInfo.canResend === true
                text: si.wait > 0 ? store.authInfo.resendHow + " in " + si.wait + " s" : store.authInfo.resendHow
                topMargin: ui.du(3); horizontalAlignment: HorizontalAlignment.Center
                textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(si.wait > 0 ? Skin.dim : Skin.link) }
                gestureHandlers: [ TapHandler { onTapped: if (si.wait <= 0) store.resendCode() } ]
            }
        }
        // --- two-step verification password
        Container {
            visible: si.step === "password"
            Label { text: "Your password"; textStyle { base: SystemDefaults.TextStyles.BigText; color: Color.create(Skin.text) } }
            Label { multiline: true; text: "This account is protected with two-step verification." + (store.authInfo.hint ? " Hint: " + store.authInfo.hint : ""); textStyle.base: body.style }
            TextField {
                id: pass; hintText: "Password"; inputMode: TextFieldInputMode.Password; topMargin: ui.du(3)
                input { submitKey: SubmitKey.Done; onSubmitted: { si.busy = true; store.checkPassword(pass.text); } }
            }
            Button { text: "Sign in"; topMargin: ui.du(3); horizontalAlignment: HorizontalAlignment.Fill; onClicked: { si.busy = true; store.checkPassword(pass.text); } }
        }
        // --- anything else
        Label {
            visible: si.step === "other"
            multiline: true
            text: store.authInfo.text ? store.authInfo.text : ""
            textStyle.base: body.style
        }
        Label {
            id: err
            property string msg: store.authError
            onMsgChanged: if (msg !== "") si.busy = false
            visible: msg !== ""
            multiline: true
            text: msg
            topMargin: ui.du(3)
            textStyle { base: SystemDefaults.TextStyles.BodyText; color: Color.create(Skin.error) }
        }
    }
}
