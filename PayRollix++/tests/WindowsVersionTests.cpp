#include "WindowsVersion.h"
// Compile-time checks for the exact policy used by RtlGetVersion at startup.
static_assert(platform::SupportedWindows(10, 0, 22000, 1), "Windows 11 initial release");
static_assert(platform::SupportedWindows(10, 0, 26100, 1), "Windows 11 newer release");
static_assert(!platform::SupportedWindows(10, 0, 21999, 1), "Reject Windows 10");
static_assert(!platform::SupportedWindows(10, 0, 19045, 1), "Reject Windows 10 22H2");
static_assert(!platform::SupportedWindows(10, 0, 26100, 3), "Reject Windows Server");
static_assert(!platform::SupportedWindows(10, 0, 26100, 2), "Reject domain controller");
static_assert(!platform::SupportedWindows(6, 3, 9600, 1), "Reject Windows 8.1");
static_assert(!platform::SupportedWindows(11, 0, 30000, 1), "Do not assume future OS support");
int main() { return 0; }
