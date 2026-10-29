import QtQuick

Canvas {
    id: icon
    required property string iconId
    required property color iconColor
    readonly property real weight: 1.8
    implicitWidth: 20
    implicitHeight: 20
    antialiasing: true

    onIconIdChanged: requestPaint()
    onIconColorChanged: requestPaint()

    function roundedRectangle(context, x, y, width, height, radius) {
        context.beginPath()
        context.moveTo(x + radius, y)
        context.lineTo(x + width - radius, y)
        context.arc(x + width - radius, y + radius, radius, -Math.PI / 2, Math.PI / 2)
        context.lineTo(x + radius, y + height)
        context.arc(x + radius, y + height - radius, radius, Math.PI / 2, Math.PI * 1.5)
        context.closePath()
    }

    onPaint: {
        const context = getContext("2d")
        context.reset()
        context.strokeStyle = icon.iconColor
        context.fillStyle = icon.iconColor
        context.lineWidth = icon.weight
        context.lineCap = "round"
        context.lineJoin = "round"

        if (iconId === "keys") {
            context.beginPath()
            context.moveTo(4.5, 4.5)
            context.lineTo(11.5, 4.5)
            context.moveTo(7, 15.5)
            context.lineTo(15.5, 15.5)
            context.stroke()
            roundedRectangle(context, 3, 7, 6, 6, 1.5)
            context.stroke()
            roundedRectangle(context, 10.5, 7, 6, 6, 1.5)
            context.stroke()
        } else if (iconId === "macro") {
            context.beginPath()
            context.moveTo(5, 5.5)
            context.lineTo(13.5, 5.5)
            context.moveTo(5, 10)
            context.lineTo(16, 10)
            context.moveTo(5, 14.5)
            context.lineTo(12, 14.5)
            context.moveTo(15, 5.5)
            context.lineTo(17, 7.5)
            context.lineTo(15, 9.5)
            context.stroke()
        } else if (iconId === "driver") {
            context.lineWidth = 1.7
            roundedRectangle(context, 3.5, 4, 12, 8.5, 2)
            context.stroke()
            context.beginPath()
            context.moveTo(6, 14)
            context.lineTo(13, 14)
            context.moveTo(9.5, 12.5)
            context.lineTo(9.5, 16.5)
            context.moveTo(17, 6)
            context.lineTo(18.5, 7.5)
            context.moveTo(17, 10.5)
            context.lineTo(18.5, 9)
            context.stroke()
        } else if (iconId === "settings") {
            context.beginPath()
            for (let i = 0; i < 8; ++i) {
                const angle = i * Math.PI / 4
                context.moveTo(10 + 7.5 * Math.sin(angle), 10 - 7.5 * Math.cos(angle))
                context.lineTo(10 + 5.7 * Math.sin(angle), 10 - 5.7 * Math.cos(angle))
            }
            context.stroke()
            context.beginPath()
            context.ellipse(10, 10, 5.4, 5.4, 0, 0, Math.PI * 2)
            context.stroke()
            context.beginPath()
            context.ellipse(10, 10, 1.8, 1.8, 0, 0, Math.PI * 2)
            context.stroke()
        } else if (iconId === "about") {
            context.beginPath()
            context.ellipse(10, 10, 7, 7, 0, 0, Math.PI * 2)
            context.moveTo(10, 9.4)
            context.lineTo(10, 14.5)
            context.stroke()
            context.beginPath()
            context.ellipse(10, 6.2, 1.05, 1.05, 0, 0, Math.PI * 2)
            context.fill()
        }
    }
}
