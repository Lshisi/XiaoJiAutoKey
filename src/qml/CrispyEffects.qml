pragma Singleton
import QtQuick
import QtQuick.Controls.Basic
import XiaoJi.UI

QtObject {
    readonly property Component burstComponent: Component { CrispyClickBurst {} }

    function burst(control, position, small = true) {
        if (!Theme.crispyThemeEnabled || !Theme.motionEnabled || !control) return
        const overlay = control.Overlay.overlay
        if (!overlay) return
        const origin = control.mapToItem(overlay, position.x, position.y)
        burstComponent.createObject(overlay, { x: origin.x - 32, y: origin.y - 32, small: small })
    }
}
