import QtQuick
import QtQuick.Layouts
import XiaoJi.UI
import "ui" as Pet

QuietCard {
    id: root
    required property QtObject pluginApi
    ColumnLayout {
        anchors.fill: parent
        spacing: Theme.cardGap
        QuietHeading { text: qsTr("江湖桌宠"); pageTitle: true; Layout.fillWidth: true }
        QuietScrollView {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            showScrollBar: false
            contentWidth: availableWidth
            contentHeight: content.implicitHeight
            ColumnLayout {
                id: content
                width: scroll.availableWidth
                spacing: Theme.cardGap
                Pet.PetOverview { pluginApi: root.pluginApi; Layout.fillWidth: true }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                Pet.PetSettings { pluginApi: root.pluginApi; Layout.fillWidth: true }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: Theme.divider }
                Pet.PetWindowSettings { pluginApi: root.pluginApi; Layout.fillWidth: true }
                QuietLabel {
                    Layout.fillWidth: true
                    visible: text.length > 0
                    text: root.pluginApi.runtime.error || ""
                    color: Theme.accent
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
