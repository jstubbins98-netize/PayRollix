#pragma once
#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace payroll {
// Writes a real Office Open XML workbook without Excel, COM, or external tools.
// Column indexes are zero-based; all other cells are explicitly literal text.
void ExportExcel(std::filesystem::path const& path,
                 std::vector<std::wstring> const& headers,
                 std::vector<std::vector<std::wstring>> const& rows,
                 std::vector<size_t> const& numericColumns = {});
// ODF 1.2 spreadsheet package for LibreOffice Calc; same typed-column contract.
void ExportOds(std::filesystem::path const& path,
               std::vector<std::wstring> const& headers,
               std::vector<std::vector<std::wstring>> const& rows,
               std::vector<size_t> const& numericColumns = {});
}
