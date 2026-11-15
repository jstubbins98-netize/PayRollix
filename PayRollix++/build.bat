@echo off
setlocal
rem Allow build.ps1 for this PowerShell process only; do not change saved policy.
rem All arguments, including quoted Qt paths, are forwarded to build.ps1.
set "exitCode=1"
set "powershellExe=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%~dp0build.ps1" (
    echo ERROR: build.ps1 was not found beside this launcher.
    goto finish
)
if not exist "%powershellExe%" (
    echo ERROR: Windows PowerShell 5.1 is required.
    goto finish
)
"%powershellExe%" -NoLogo -NoProfile -ExecutionPolicy Bypass -File "%~dp0build.ps1" %*
set "exitCode=%errorlevel%"

:finish
if not "%exitCode%"=="0" echo Build failed. Review the error above and the prerequisites in README.txt.
rem Keep the result visible when double-clicked without command-line arguments.
if "%~1"=="" pause
exit /b %exitCode%
