#!/usr/bin/env python3
"""Cross-platform structural checks, not a native Windows build or UI test."""
from pathlib import Path
import re
import sqlite3
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}


def main():
    assert (ROOT / "README.md").read_bytes() == (ROOT / "README.txt").read_bytes(), "Manuals differ"
    project = ET.parse(ROOT / "Payrollix.vcxproj")
    ET.parse(ROOT / "app.manifest")
    packages = ET.parse(ROOT / "packages.config")
    names = {}
    for package in packages.getroot():
        names[package.attrib["id"]] = package.attrib["version"]
    assert "Microsoft.WindowsAppSDK.WinUI" in names
    assert "Microsoft.WindowsAppSDK.Runtime" in names
    sources = []
    for tag in ("ClCompile", "ClInclude", "None"):
        for element in project.findall(f".//m:{tag}[@Include]", NS):
            path = ROOT / element.attrib["Include"].replace("\\", "/")
            assert path.is_file(), f"Missing project file: {path}"
            if tag == "ClCompile":
                sources.append(path)
    imports = []
    for element in project.findall(".//m:Import", NS):
        value = element.attrib["Project"]
        if value.startswith("packages\\"):
            _, directory, remainder = value.split("\\", 2)
            assert any(directory == f"{name}.{version}" for name, version in names.items()), f"Unpinned import: {directory}"
            imports.append(value)
    for name, version in names.items():
        assert any(f"{name}.{version}\\" in value for value in imports), f"Package has no build import: {name}"
    assert not any("SecurityTestDouble" in str(source) for source in sources), "Test password provider in application"
    for source in [*sources, *ROOT.glob("include/*.h")]:
        text = source.read_text(encoding="utf-8")
        assert not re.search(r"#\s*include\s*[<\"]Q\w+", text), f"Qt dependency: {source}"
    configured = {path.name for path in sources}
    assert {path.name for path in (ROOT / "src").glob("*.cpp")} == configured, "Unbuilt source files"
    assert project.find(".//m:WindowsTargetPlatformMinVersion", NS).text == "10.0.22000.0"
    assert project.find(".//m:PlatformToolset", NS).text == "v145", "Use the VS 2026 C++ toolset"
    solution = (ROOT / "Payrollix.sln").read_text()
    assert "# Visual Studio Version 18\n" in solution
    assert "VisualStudioVersion = 18." in solution
    build_script = (ROOT / "build.ps1").read_text()
    assert '-version "[18.0,19.0)"' in build_script, "Discover VS 2026, not older MSBuild"
    workflow = (ROOT.parent / ".github/workflows/windows-build.yml").read_text()
    assert "runs-on: windows-2025-vs2026" in workflow
    assert '-G "Visual Studio 18 2026" -A x64 -T v145' in workflow
    platform = (ROOT / "src/Platform.cpp").read_text()
    assert "version.dwBuildNumber >= 22000" in platform and "VER_NT_WORKSTATION" in platform
    main_source = (ROOT / "src/main.cpp").read_text()
    assert main_source.index("platform::Windows11()") < main_source.index("Application::Start(")
    assert "Qt6" not in (ROOT / "CMakeLists.txt").read_text()
    schema_text = (ROOT / "src/Schema.h").read_text()
    schema = re.search(r'R"SQL\((.*?)\)SQL"', schema_text, re.S).group(1)
    with sqlite3.connect(":memory:") as db:
        db.executescript(schema)
        db.executescript(schema)
        tables = {r[0] for r in db.execute("SELECT name FROM sqlite_master WHERE type='table'")}
        assert {"users", "payroll", "departments", "pay_periods", "employment_history", "audit_log",
                "payslips", "winui_payslip_details"}.issubset(tables)
        assert db.execute("SELECT COUNT(*) FROM users").fetchone()[0] == 0
    print("PASS: project files, VS 2026 toolchain, pinned imports, Win11 gate, Qt removal, schema, and identical manuals.")
    print("This is NOT a Windows compiler or WinUI runtime check.")


if __name__ == "__main__":
    main()
