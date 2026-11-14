#pragma once
namespace platform {
constexpr bool SupportedWindows(unsigned long major, unsigned long minor,
                                unsigned long build, unsigned char productType) {
    // VER_NT_WORKSTATION == 1. Windows 11 still reports NT 10.0.
    return major == 10 && minor == 0 && build >= 22000 && productType == 1;
}
}
