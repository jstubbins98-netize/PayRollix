#!/usr/bin/env bash
set -eu
printf '%s\n' \
  "Payrollix++ uses Qt 6 Widgets and is exclusive to Windows 11." \
  "macOS/Linux application builds are not supported. No packages were installed." \
  "On Windows 11: run .\\build.ps1 -QtRoot C:\\Qt\\6.9.3\\msvc2022_64 in PowerShell." \
  "Portable core checks only: python3 tests/check_project.py"
exit 1
