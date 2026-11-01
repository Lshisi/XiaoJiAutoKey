import QtQuick
import QtQuick.Layouts
import XiaoJi.UI

FocusScope {
    id: guide
    required property Item pageRoot
    readonly property var guideState: onboardingModel.state
    readonly property Item targetItem: pageRoot && pageRoot.guideTargets
        ? pageRoot.guideTargets[guideState.target] || null : null
    // Read ancestor geometry so scrolling and layout changes refresh mapToItem too.
    readonly property var targetGeometry: {
        const geometry = []
        let item = targetItem
        while (item) {
            geometry.push(Qt.rect(item.x, item.y, item.width, item.height))
            item = item.parent
        }
        return geometry
    }
    readonly property rect targetRect: {
        if (targetGeometry.length === 0 || !targetItem || !targetItem.visible)
            return Qt.rect(0, 0, 0, 0)
        const point = targetItem.mapToItem(guide, 0, 0)
        const left = Math.max(0, point.x - 4)
        const top = Math.max(0, point.y - 4)
        const right = Math.min(width, point.x + targetItem.width + 4)
        const bottom = Math.min(height, point.y + targetItem.height + 4)
        return Qt.rect(left, top, Math.max(0, right - left), Math.max(0, bottom - top))
    }
    readonly property bool hasTarget: !guideState.welcome && targetRect.width > 0 && targetRect.height > 0
    readonly property bool cardBesideTarget: hasTarget
        && (targetRect.x >= card.width + Theme.cardGap
            || width - targetRect.x - targetRect.width >= card.width + Theme.cardGap)
    readonly property real spaceAboveTarget: Math.max(0, targetRect.y - Theme.pageMargin - Theme.cardGap)
    readonly property real spaceBelowTarget: Math.max(0,
        height - targetRect.y - targetRect.height - Theme.pageMargin - Theme.cardGap)
    focus: true
    Accessible.role: Accessible.Dialog
    Accessible.name: guideState.title
    Accessible.description: guideState.body
    readonly property string stepKey: guideState.progress
    function focusGuideControl() {
        if (!visible) return
        if (nextButton.enabled) nextButton.forceActiveFocus(Qt.TabFocusReason)
        else actionButton.forceActiveFocus(Qt.TabFocusReason)
    }
    Component.onCompleted: Qt.callLater(guide.focusGuideControl)
    onVisibleChanged: if (visible) Qt.callLater(guide.focusGuideControl)
    onStepKeyChanged: Qt.callLater(guide.focusGuideControl)
    Keys.onEscapePressed: event => { onboardingModel.skip(); event.accepted = true }
    Repeater {
        model: guide.hasTarget ? [
            Qt.rect(0, 0, guide.width, guide.targetRect.y),
            Qt.rect(0, guide.targetRect.y, guide.targetRect.x, guide.targetRect.height),
            Qt.rect(guide.targetRect.x + guide.targetRect.width, guide.targetRect.y,
                guide.width - guide.targetRect.x - guide.targetRect.width, guide.targetRect.height),
            Qt.rect(0, guide.targetRect.y + guide.targetRect.height, guide.width,
                guide.height - guide.targetRect.y - guide.targetRect.height)
        ] : [Qt.rect(0, 0, guide.width, guide.height)]
        delegate: Rectangle {
            required property var modelData
            x: modelData.x
            y: modelData.y
            width: modelData.width
            height: modelData.height
            color: Qt.alpha(Theme.background, Theme.dark ? 0.8 : 0.76)
            MouseArea {
                anchors.fill: parent
                acceptedButtons: Qt.AllButtons
                onWheel: wheel => { wheel.accepted = true }
            }
            Accessible.ignored: true
        }
    }
    Rectangle {
        x: guide.targetRect.x
        y: guide.targetRect.y
        width: guide.targetRect.width
        height: guide.targetRect.height
        visible: guide.hasTarget
        radius: Theme.buttonRadius
        color: "transparent"
        border.width: 2
        border.color: Theme.accent
        Accessible.ignored: true
    }
    QuietCard {
        id: card
        width: Math.min(320, guide.width - 2 * Theme.pageMargin)
        height: Math.min(content.implicitHeight + topPadding + bottomPadding,
            guide.height - 2 * Theme.pageMargin,
            !guide.hasTarget || guide.cardBesideTarget ? guide.height
                : Math.max(guide.spaceAboveTarget, guide.spaceBelowTarget))
        x: {
            if (!guide.hasTarget) return (guide.width - width) / 2
            if (guide.targetRect.x >= width + Theme.cardGap)
                return guide.targetRect.x - Theme.cardGap - width
            if (guide.width - guide.targetRect.x - guide.targetRect.width >= width + Theme.cardGap)
                return guide.targetRect.x + guide.targetRect.width + Theme.cardGap
            return Math.max(Theme.pageMargin, Math.min(guide.width - width - Theme.pageMargin,
                guide.targetRect.x + guide.targetRect.width / 2 - width / 2))
        }
        y: {
            let position = (guide.height - height) / 2
            if (guide.hasTarget) {
                if (guide.cardBesideTarget) position = guide.targetRect.y
                else if (guide.spaceAboveTarget >= height)
                    position = guide.targetRect.y - height - Theme.cardGap
                else position = guide.targetRect.y + guide.targetRect.height + Theme.cardGap
            }
            return Math.max(Theme.pageMargin, Math.min(guide.height - height - Theme.pageMargin, position))
        }
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.AllButtons
            onWheel: wheel => { wheel.accepted = true }
            Accessible.ignored: true
        }
        ColumnLayout {
            id: content
            anchors.fill: parent
            spacing: Theme.cardGap
            RowLayout {
                Layout.fillWidth: true
                QuietLabel { Layout.fillWidth: true; text: guide.guideState.progress; color: Theme.muted }
                ActionButton {
                    text: qsTr("跳过")
                    quiet: true
                    onClicked: onboardingModel.skip()
                    hint: qsTr("以后可在关于软件中重新查看新手引导")
                }
            }
            QuietHeading { Layout.fillWidth: true; text: guide.guideState.title; Accessible.role: Accessible.Heading }
            QuietScrollView {
                id: descriptionScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredHeight: descriptions.implicitHeight
                rightPadding: 8
                contentWidth: availableWidth
                contentHeight: descriptions.implicitHeight
                ColumnLayout {
                    id: descriptions
                    width: descriptionScroll.availableWidth
                    spacing: Theme.cardGap
                    QuietLabel { Layout.fillWidth: true; text: guide.guideState.body }
                    QuietLabel { Layout.fillWidth: true; text: guide.guideState.hint; color: Theme.muted }
                }
            }
            ActionButton {
                id: actionButton
                Layout.fillWidth: true
                visible: !guide.guideState.welcome && guide.guideState.actionText.length > 0
                text: guide.guideState.actionText
                onClicked: onboardingModel.act()
            }
            ActionButton {
                Layout.fillWidth: true
                visible: guide.guideState.secondaryText.length > 0
                text: guide.guideState.secondaryText
                quiet: true
                onClicked: onboardingModel.secondaryAction()
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                ActionButton {
                    visible: guide.guideState.canPrevious
                    text: qsTr("上一步")
                    onClicked: onboardingModel.previous()
                }
                Item { Layout.fillWidth: true }
                ActionButton {
                    id: nextButton
                    primary: true
                    text: guide.guideState.welcome ? qsTr("开始引导") : guide.guideState.nextText
                    enabled: guide.guideState.welcome || guide.guideState.canNext
                    onClicked: guide.guideState.welcome ? onboardingModel.begin() : onboardingModel.next()
                }
            }
        }
    }
}
