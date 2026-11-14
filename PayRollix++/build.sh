#!/usr/bin/env bash
set -eu
printf '%s\n' \
  "Payrollix++ now uses Microsoft WinUI 3 and is exclusive to Windows 11." \
  "macOS/Linux application builds are not supported. No packages were installed." \
  "On Windows 11: open PowerShell in PayRollix++ and run .\\build.ps1" \
  "Portable core checks only: python3 tests/check_project.py"
exit 1
