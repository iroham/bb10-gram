// Pic: an ImageView that follows a file address that may be empty (rows are reused for other items).
import bb.cascades 1.3

ImageView {
    id: pic
    property string src: ""
    function apply() { if (pic.src) pic.imageSource = pic.src; else pic.resetImageSource(); }
    onSrcChanged: apply()
    onCreationCompleted: apply()
    scalingMethod: ScalingMethod.AspectFill
}
