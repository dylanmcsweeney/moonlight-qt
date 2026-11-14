#pragma once

#include <QRect>

namespace WMUtils {

// SDL backends such as Wayland can report the full display bounds as the
// usable bounds. Treat the usable area as authoritative only if at least one
// display reports a distinct rectangle. The callback returns false when SDL
// cannot query either rectangle for that display.
template<typename GetDisplayBounds>
bool hasAnyDisplayUsableBounds(int displayCount, GetDisplayBounds getDisplayBounds)
{
    for (int displayIndex = 0; displayIndex < displayCount; ++displayIndex) {
        QRect displayBounds;
        QRect usableBounds;
        if (getDisplayBounds(displayIndex, &displayBounds, &usableBounds) &&
                displayBounds != usableBounds) {
            return true;
        }
    }

    return false;
}

}
