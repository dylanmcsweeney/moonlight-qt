import QtQuick 2.9
import QtQuick.Controls 2.2

GridView {
    property int minMargin: 10
    property real itemWidth: cellWidth
    property real itemHeight: cellHeight
    property real availableWidth: parent.width - 2 * minMargin
    property real horizontalCellGap: Math.max(0, cellWidth - itemWidth)
    property int itemsPerRow: Math.max(
                                  1, Math.floor(
                                      (availableWidth + horizontalCellGap) /
                                      cellWidth))
    property int visibleColumns: Math.max(
                                     1, Math.min(count, itemsPerRow))
    property real visibleRowWidth:
        (visibleColumns - 1) * cellWidth + itemWidth
    property real horizontalMargin:
        Math.max(minMargin, (parent.width - visibleRowWidth) / 2)
    property real trailingMargin:
        Math.max(0, parent.width - horizontalMargin -
                    visibleColumns * cellWidth)

    function updateMargins() {
        leftMargin = horizontalMargin
        // GridView wraps according to cellWidth, including the unused spacing
        // after the final delegate. Exclude that invisible tail from the right
        // margin so the visible delegates remain centered without wrapping.
        rightMargin = trailingMargin

        // GridView can retain the content position associated with an earlier
        // margin while the model and window geometry settle during startup.
        // This view only scrolls vertically, so keep its horizontal origin
        // aligned with the current left margin and rebuild the cell layout.
        contentX = -leftMargin
        forceLayout()
    }

    onHorizontalMarginChanged: {
        updateMargins()
    }

    onTrailingMarginChanged: {
        updateMargins()
    }

    onCountChanged: {
        updateMargins()
    }

    onWidthChanged: {
        updateMargins()
    }

    Component.onCompleted: {
        updateMargins()
    }

    boundsBehavior: Flickable.OvershootBounds
}
