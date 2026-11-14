#Requires -Version 5.1
[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release")][string]$Configuration = "Release",
    [switch]$Launch,
    [switch]$SkipHostCheck
)
$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

if ($env:OS -ne "Windows_NT") { throw "Payrollix++ requires Windows 11. This script must run on Windows." }
$osInfo = Get-CimInstance Win32_OperatingSystem
if (-not $SkipHostCheck -and ([int]$osInfo.BuildNumber -lt 22000 -or [int]$osInfo.ProductType -ne 1 -or [Environment]::OSVersion.Version.Major -ne 10)) {
    throw "Use a Windows 11 client workstation. -SkipHostCheck is for compile-only CI hosts; the app always enforces Windows 11 at startup."
}
if (-not [Environment]::Is64BitOperatingSystem) { throw "An x64-capable Windows installation is required." }
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    throw "Install Visual Studio 2022 with Desktop development with C++ (v143), Windows 11 SDK 10.0.26100.0, and WinUI C++ application development tools."
}
$found = @(& $vswhere -latest -version "[17.0,18.0)" -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find "MSBuild\**\Bin\MSBuild.exe")
if ($found.Count -eq 0) { throw "Visual Studio 2022 MSBuild / C++ tools were not found." }
$msbuild = $found[0]
$nuget = Get-Command nuget.exe -ErrorAction SilentlyContinue
if ($nuget) { $nugetExe = $nuget.Source }
else {
    $tools = Join-Path $env:LOCALAPPDATA "PayrollixBuildTools"
    New-Item -ItemType Directory -Path $tools -Force | Out-Null
    $nugetExe = Join-Path $tools "nuget.exe"
    if (-not (Test-Path $nugetExe)) {
        Write-Host "Downloading the NuGet CLI from Microsoft's official distribution endpoint..."
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest "https://dist.nuget.org/win-x86-commandline/v6.14.0/nuget.exe" -OutFile $nugetExe -UseBasicParsing
    }
}
Push-Location $PSScriptRoot
try {
    Write-Host "Restoring pinned Windows App SDK and C++/WinRT dependencies..."
    & $nugetExe restore packages.config -PackagesDirectory packages -Source "https://api.nuget.org/v3/index.json" -NonInteractive
    if ($LASTEXITCODE -ne 0) { throw "NuGet restore failed ($LASTEXITCODE)." }
    & $msbuild Payrollix.vcxproj /m /t:Build "/p:Configuration=$Configuration" /p:Platform=x64 /verbosity:minimal /nologo
    if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)." }
    $output = Join-Path $PSScriptRoot "bin\x64\$Configuration"
    Write-Host "Build succeeded: $output\PayrollApp.exe" -ForegroundColor Green
    Write-Host "Distribute the ENTIRE output folder, not only the executable. Target PCs also require the Microsoft Visual C++ 2015-2022 x64 Redistributable."
    if ($Launch) {
        if ($SkipHostCheck) { throw "Launch is disabled when -SkipHostCheck is used." }
        Start-Process (Join-Path $output "PayrollApp.exe") -WorkingDirectory $output
    }
}
finally { Pop-Location }
