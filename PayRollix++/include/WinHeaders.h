#pragma once
#ifndef _WIN32
#error Payrollix++ uses WinUI 3 and must be compiled for Windows.
#endif
#include <windows.h>
// This legacy Win32 macro collides with WinRT animation method names.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
