import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import XiaoJi.UI

ColumnLayout {
    id: report
    readonly property var fields: dialogModel.dialogState
    readonly property color statusColor: fields.severity === 3 ? Theme.danger : fields.severity === 2 ? Theme.warning : fields.severity === 1 ? Theme.success : Theme.text
    readonly property real preferredWidth: Math.max(reportHeading.implicitWidth, bodyMeasure.implicitWidth, statusLabel.implicitWidth)
    spacing: 10
    Text {
        id: bodyMeasure
        visible: false
        text: bodyLabel.text
        textFormat: bodyLabel.textFormat
        font: bodyLabel.font
        renderType: bodyLabel.renderType
        renderTypeQuality: bodyLabel.renderTypeQuality
        wrapMode: Text.NoWrap
        Accessible.ignored: true
    }
    QuietHeading { id: reportHeading; visible: text.length > 0; Layout.fillWidth: true; text: report.fields.heading; color: report.statusColor }
    QuietLabel {
        id: bodyLabel
        Layout.fillWidth: true
        text: report.fields.body
        textFormat: report.fields.bodyRich ? Text.RichText : Text.PlainText
        color: Theme.muted
        wrapMode: Text.Wrap
    }
    QuietLabel { id: statusLabel; visible: text.length > 0; Layout.fillWidth: true; text: report.fields.status; color: report.statusColor }
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
