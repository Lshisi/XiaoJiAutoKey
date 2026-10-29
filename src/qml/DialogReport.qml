import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: report
    readonly property var fields: dialogModel.dialogState
    readonly property color statusColor: fields.severity === 3 ? Theme.danger : fields.severity === 2 ? Theme.warning : fields.severity === 1 ? Theme.success : Theme.text
    spacing: 10
    QuietHeading { visible: text.length > 0; Layout.fillWidth: true; text: report.fields.heading; color: report.statusColor }
    QuietLabel {
        Layout.fillWidth: true
        text: report.fields.body
        textFormat: report.fields.bodyRich ? Text.RichText : Text.PlainText
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    QuietLabel { visible: text.length > 0; Layout.fillWidth: true; text: report.fields.status; color: report.statusColor }
    Loader {
        Layout.fillWidth: true
        active: report.fields.progressVisible
        visible: active
        sourceComponent: ColumnLayout {
            Layout.fillWidth: true
            spacing: 8
            BusyIndicator { visible: report.fields.progressIndeterminate; running: visible; Layout.alignment: Qt.AlignHCenter; implicitWidth: 28; implicitHeight: 28 }
            QuietProgress { visible: !report.fields.progressIndeterminate; Layout.fillWidth: true; value: report.fields.progressValue }
            QuietLabel { Layout.fillWidth: true; text: report.fields.progressText; color: Theme.muted; font.pixelSize: 12 }
        }
    }
    Item { Layout.fillHeight: true }
}
