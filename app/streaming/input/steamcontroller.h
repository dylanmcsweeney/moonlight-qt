#pragma once

#include <cstdint>

namespace GamepadUtils {

constexpr bool isKnownSteamController(uint16_t vendorId, uint16_t productId)
{
    if (vendorId != 0x28de) {
        return false;
    }

    switch (productId) {
    case 0x1101:
    case 0x1102:
    case 0x1105:
    case 0x1106:
    case 0x1142:
    case 0x1201:
    case 0x1202:
    case 0x1205:
    case 0x1302:
    case 0x1303:
    case 0x1304:
    case 0x1305:
        return true;
    default:
        return false;
    }
}

}
