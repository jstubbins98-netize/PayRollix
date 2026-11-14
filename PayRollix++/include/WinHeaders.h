#pragma once
#ifndef _WIN32
#error Payrollix++ is exclusive to Windows 11 and must be compiled for Windows.
#endif
#include <windows.h>
// Avoid exporting legacy macros into application and Qt headers.
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
