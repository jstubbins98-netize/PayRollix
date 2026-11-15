#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [string]$QtRoot,
    [switch]$Launch,
    [switch]$SkipHostCheck
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
if ($env:OS -ne "Windows_NT") { throw "Payrollix++ requires Windows 11. Run this script on Windows." }
if ($Launch -and $SkipHostCheck) { throw "Launch is disabled with -SkipHostCheck." }
$osInfo = Get-CimInstance Win32_OperatingSystem
if (-not $SkipHostCheck -and ([int]$osInfo.BuildNumber -lt 22000 -or [int]$osInfo.ProductType -ne 1 -or [Environment]::OSVersion.Version.Major -ne 10)) {
    throw "Use Windows 11 client. -SkipHostCheck is for compile-only CI; the app always enforces Windows 11."
}
if (-not [Environment]::Is64BitOperatingSystem) { throw "An x64-capable Windows installation is required." }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { throw "Install Visual Studio 2026, Desktop development with C++, v145 and a Windows 11 SDK." }
$installations = @(& $vswhere -latest -version "[18.0,19.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath)
if ($LASTEXITCODE -ne 0 -or $installations.Count -eq 0) { throw "Visual Studio 2026 C++ tools were not found." }
$cmake = Get-Command cmake.exe -ErrorAction SilentlyContinue
if (-not $cmake) { throw "Install CMake 4.1+ (included with Visual Studio's C++ CMake tools) and add it to PATH." }
$build = Join-Path $PSScriptRoot "build"
$qtResult = Join-Path $build "qt-root.txt"
& $cmake.Source "-DPAYROLLIX_QT_ROOT=$QtRoot" "-DPAYROLLIX_QT_RESULT=$qtResult" -P (Join-Path $PSScriptRoot "cmake\FindPayrollQt.cmake")
if ($LASTEXITCODE -ne 0) { throw "Qt detection failed. See the installation instructions above; the build has not started." }
$QtRoot = (Get-Content -LiteralPath $qtResult -Raw -Encoding UTF8).Trim()
& $cmake.Source -S $PSScriptRoot -B $build -G "Visual Studio 18 2026" -A x64 -T v145 "-DCMAKE_GENERATOR_INSTANCE=$($installations[0])" "-DPAYROLLIX_QT_ROOT=$QtRoot"
if ($LASTEXITCODE -ne 0) { throw "CMake configuration failed ($LASTEXITCODE)." }
& $cmake.Source --build $build --config $Configuration --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
$output = Join-Path $PSScriptRoot "bin\x64\$Configuration"
$deploy = Join-Path $QtRoot "bin\windeployqt.exe"
if (-not (Test-Path $deploy)) { throw "Qt deployment tool missing: $deploy" }
$deployMode = "--release"
if ($Configuration -eq "Debug") { $deployMode = "--debug" }
# windeployqt may need the MSVC tools for compiler-runtime discovery.
$devcmd = Join-Path $installations[0] "Common7\Tools\VsDevCmd.bat"
$command = "`"$devcmd`" -arch=x64 -host_arch=x64 >nul && `"$deploy`" $deployMode --no-translations `"$output\PayrollApp.exe`""
& $env:ComSpec /d /s /c "`"$command`""
if ($LASTEXITCODE -ne 0) { throw "Qt deployment failed ($LASTEXITCODE)." }
Write-Host "Built and deployed Qt runtime files to $output"
Write-Host "Distribute the ENTIRE folder with Qt license notices; install the latest x64 Visual C++ v14 Redistributable."
if ($Launch) { Start-Process (Join-Path $output "PayrollApp.exe") -WorkingDirectory $output }
