// FaceGate — drop-in face-lock UI. Give it a FaceLock instance (`lock`) and call
// verify() or enroll(); it shows the live front-camera preview, a status line, a
// blink prompt and (for enrol) sample progress, and re-emits the outcome.
//
//   FaceGate { id: gate; lock: faceLock; onUnlocked: ...; onFailed: ... }
//   Component.onCompleted: gate.verify()
import bb.cascades 1.3

Container {
    id: gate
    property variant lock
    property int previewSize: 720            // square preview edge, px
    property variant accent: Color.create("#4fc3f7")

    signal unlocked(int distance)
    signal enrolled()
    signal failed(string reason)

    function verify() { lock.setPreviewWindow(preview.windowGroup, preview.windowId); lock.startVerify(); }
    function enroll() { lock.setPreviewWindow(preview.windowGroup, preview.windowId); lock.startEnroll(); }
    function stop()   { lock.cancel(); }

    layout: DockLayout {}
    horizontalAlignment: HorizontalAlignment.Fill
    verticalAlignment: VerticalAlignment.Fill

    // --- live camera preview ---
    Container {
        horizontalAlignment: HorizontalAlignment.Center
        verticalAlignment: VerticalAlignment.Center
        layout: DockLayout {}

        ForeignWindowControl {
            id: preview
            objectName: "glancePreview"
            windowId: "glancePreview"
            updatedProperties: WindowProperty.Position | WindowProperty.Size | WindowProperty.Visible
            visible: boundToWindow
            preferredWidth: gate.previewSize
            preferredHeight: gate.previewSize
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
        }

        // ring that turns accent when a face is locked on
        Container {
            preferredWidth: gate.previewSize
            preferredHeight: gate.previewSize
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
            background: Color.Transparent
            opacity: preview.boundToWindow ? 1.0 : 0.0
            property bool face: gate.lock ? gate.lock.faceSeen : false
            // a simple inset border via a bordered image-less container
            Container {
                horizontalAlignment: HorizontalAlignment.Fill
                verticalAlignment: VerticalAlignment.Fill
            }
        }

        // placeholder while the camera window hasn't bound yet
        Label {
            visible: !preview.boundToWindow
            text: "Starting camera…"
            textStyle.color: Color.LightGray
            horizontalAlignment: HorizontalAlignment.Center
            verticalAlignment: VerticalAlignment.Center
        }
    }

    // --- status overlay (bottom) ---
    Container {
        horizontalAlignment: HorizontalAlignment.Fill
        verticalAlignment: VerticalAlignment.Bottom
        topPadding: 24; bottomPadding: 40; leftPadding: 32; rightPadding: 32
        background: Color.create("#cc101418")

        Label {
            id: statusLabel
            text: gate.lock ? gate.lock.status : ""
            horizontalAlignment: HorizontalAlignment.Center
            textStyle.color: Color.White
            textStyle.fontSize: FontSize.Large
            textStyle.textAlign: TextAlign.Center
        }
        // blink hint, pulses when a blink is requested
        Label {
            id: blinkHint
            visible: false
            text: "👁  Blink"
            horizontalAlignment: HorizontalAlignment.Center
            textStyle.color: gate.accent
            textStyle.fontSize: FontSize.Medium
            opacity: 0.4
            animations: [
                SequentialAnimation {
                    id: blinkPulse
                    repeatCount: AnimationRepeatCount.Forever
                    FadeTransition { fromOpacity: 0.4; toOpacity: 1.0; duration: 500 }
                    FadeTransition { fromOpacity: 1.0; toOpacity: 0.4; duration: 500 }
                }
            ]
        }
        // enrol progress
        Container {
            id: progressRow
            visible: false
            horizontalAlignment: HorizontalAlignment.Center
            topMargin: 12
            Label {
                id: progressLabel
                text: ""
                textStyle.color: Color.LightGray
                textStyle.fontSize: FontSize.Small
                horizontalAlignment: HorizontalAlignment.Center
            }
        }
    }

    // Connect to the FaceLock C++ signals via JS (the Connections element isn't
    // registered in this Cascades build).
    function _onBlinkPrompt()      { blinkHint.visible = true; blinkPulse.play(); }
    function _onBlinkOk()          { blinkHint.visible = false; blinkPulse.stop(); }
    function _onProgress(done, total) {
        progressRow.visible = true; progressLabel.text = "Captured " + done + " of " + total;
    }
    function _onSucceeded(distance){ blinkHint.visible = false; gate.unlocked(distance); }
    function _onEnrolledDone()     { blinkHint.visible = false; progressRow.visible = false; gate.enrolled(); }
    function _onFailed(reason)     { blinkHint.visible = false; progressRow.visible = false; gate.failed(reason); }

    onLockChanged: {
        if (!gate.lock) return;
        gate.lock.blinkPrompt.connect(_onBlinkPrompt);
        gate.lock.blinkOk.connect(_onBlinkOk);
        gate.lock.progress.connect(_onProgress);
        gate.lock.succeeded.connect(_onSucceeded);
        gate.lock.enrolledDone.connect(_onEnrolledDone);
        gate.lock.failed.connect(_onFailed);
    }
}
