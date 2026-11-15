#!/usr/bin/env python3
"""Cross-platform structural checks, not a native Windows build or UI test."""
from pathlib import Path
import re
import sqlite3
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]


def main():
    assert (ROOT / "README.md").read_bytes() == (ROOT / "README.txt").read_bytes(), "Manuals differ"
    ET.parse(ROOT / "app.manifest")
    cmake = (ROOT / "CMakeLists.txt").read_text()
    assert "if(NOT WIN32)" in cmake, "Application must reject non-Windows builds"
    assert "find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets)" in cmake
    assert 'CMAKE_VS_PLATFORM_TOOLSET STREQUAL "v145"' in cmake
    assert "qt_generate_deploy_app_script" in cmake
    for source in (ROOT / "src").glob("*.cpp"):
        assert f"src/{source.name}" in cmake, f"Unbuilt source file: {source}"
    assert "SecurityTestDouble" not in cmake, "Test password provider in application"
    for source in [*ROOT.glob("src/*.cpp"), *ROOT.glob("include/*.h")]:
        text = source.read_text(encoding="utf-8")
        assert not re.search(r"#\s*include\s*[<\"](?:winrt/|microsoft\.ui)", text), f"WinUI dependency: {source}"
    assert not (ROOT / "packages.config").exists(), "Stale WinUI package configuration"
    build_script = (ROOT / "build.ps1").read_text()
    assert '-version "[18.0,19.0)"' in build_script
    assert '-G "Visual Studio 18 2026" -A x64 -T v145' in build_script
    assert "windeployqt.exe" in build_script
    assert '& $deploy $deployMode --no-translations --no-compiler-runtime (Join-Path $output "PayrollApp.exe")' in build_script
    assert "$env:ComSpec" not in build_script, "Do not nest deployment inside cmd.exe quote parsing"
    assert "VsDevCmd.bat" not in build_script, "Deployment must not require a developer shell"
    assert "cmake\\FindPayrollQt.cmake" in build_script
    assert "payrollix_find_qt(_payrollix_qt)" in cmake
    launcher = (ROOT / "build.bat").read_text()
    assert '-ExecutionPolicy Bypass -File "%~dp0build.ps1" %*' in launcher
    assert 'set "exitCode=%errorlevel%"' in launcher
    assert "exit /b %exitCode%" in launcher
    assert 'if "%~1"=="" pause' in launcher
    assert "Set-ExecutionPolicy" not in launcher, "Do not persistently weaken execution policy"
    workflow = (ROOT.parent / ".github/workflows/windows-build.yml").read_text()
    assert "runs-on: windows-2025-vs2026" in workflow
    assert "jurplel/install-qt-action@" in workflow
    assert '-G "Visual Studio 18 2026" -A x64 -T v145' in workflow
    platform = (ROOT / "src/Platform.cpp").read_text()
    assert "RtlGetVersion" in platform and "SupportedWindows(" in platform
    policy = (ROOT / "include/WindowsVersion.h").read_text()
    assert "build >= 22000" in policy and "productType == 1" in policy
    main_source = (ROOT / "src/main.cpp").read_text()
    assert main_source.index("platform::Windows11()") < main_source.index("QApplication application")
    assert main_source.index("platform::Windows11()") < main_source.index("AppWindow window")
    assert '#include "WinHeaders.h"' in main_source
    schema_text = (ROOT / "src/Schema.h").read_text()
    schema = re.search(r'R"SQL\((.*?)\)SQL"', schema_text, re.S).group(1)
    with sqlite3.connect(":memory:") as db:
        db.executescript(schema)
        db.executescript(schema)
        tables = {r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        assert {"users", "payroll", "departments", "pay_periods", "employment_history", "audit_log",
                "payslips", "winui_payslip_details"}.issubset(tables)
        assert db.execute("SELECT COUNT(*) FROM users").fetchone()[0] == 0
    subprocess.run([sys.executable, str(ROOT / "tests/test_qt_discovery.py")], check=True)
    print("PASS: Qt discovery/build/deployment, VS 2026 toolchain, Win11 gate, schema, and identical manuals.")
    print("This is NOT a Windows compiler or interactive runtime check.")


if __name__ == "__main__":
    main()
