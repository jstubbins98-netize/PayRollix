#include "ExcelExport.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
template<class F> void Reject(F&& action) {
    try { action(); }
    catch (std::exception const&) { return; }
    throw std::runtime_error("Invalid export was accepted");
}
}
int main(int argc, char** argv) {
    try {
        if (argc != 2) throw std::runtime_error("Supply a test output directory");
        auto directory = std::filesystem::u8path(argv[1]);
        std::vector<std::wstring> headers{L"Name", L"Amount", L"Phone", L"Notes"};
        std::vector<std::vector<std::wstring>> rows{
            {L"Ren\u00e9e \U0001f600", L"1234.50", L"0012345678", L" <&> \"quoted\"\nsecond line "},
            {L"=1+1", L"-12.25", L"+15551234567", L"_x0041_"},
            {L"@SUM(A1)", L"0", L"0000", L"\tKeep whitespace\r\n"},
            {L"-text", L"", L"2026-11-15", L"+text"}
        };
        auto target = directory / "unicode workbook.xlsx";
        payroll::ExportExcel(target, headers, rows, {1});
        payroll::ExportExcel(directory / "empty.xlsx", headers, {}, {1});
        std::vector<std::wstring> wide(28, L"Column");
        payroll::ExportExcel(directory / "wide.xlsx", wide, {wide});
        auto ods = directory / "unicode workbook.ods";
        payroll::ExportOds(ods, headers, rows, {1});
        payroll::ExportOds(directory / "empty.ods", headers, {}, {1});
        payroll::ExportOds(directory / "wide.ods", wide, {wide});
        Reject([&] { payroll::ExportOds(ods, {}, {}); });
        Reject([&] { payroll::ExportOds(ods, headers, {{L"short row"}}); });
        Reject([&] { payroll::ExportOds(ods, headers, rows, {4}); });
        Reject([&] { payroll::ExportExcel(target, {}, {}); });
        Reject([&] { payroll::ExportExcel(target, headers, {{L"short row"}}); });
        Reject([&] { payroll::ExportExcel(target, headers, rows, {4}); });
        auto bad = rows;
        bad[0][1] = L"=1+1";
        Reject([&] { payroll::ExportOds(ods, headers, bad, {1}); });
        Reject([&] { payroll::ExportExcel(target, headers, bad, {1}); });
        bad[0][1] = L"nan";
        Reject([&] { payroll::ExportOds(ods, headers, bad, {1}); });
        Reject([&] { payroll::ExportExcel(target, headers, bad, {1}); });
        bad = rows;
        bad[0][0] = std::wstring(32768, L'a');
        Reject([&] { payroll::ExportOds(ods, headers, bad); });
        Reject([&] { payroll::ExportExcel(target, headers, bad); });
        bad[0][0] = L"bad\x01";
        Reject([&] { payroll::ExportOds(ods, headers, bad); });
        Reject([&] { payroll::ExportExcel(target, headers, bad); });
        Reject([&] { payroll::ExportExcel(directory / "missing" / "cannot-save.xlsx", headers, rows); });
        Reject([&] { payroll::ExportOds(directory / "missing" / "cannot-save.ods", headers, rows); });
        // Failures above must leave the valid workbook at target intact.
        std::ifstream file(target, std::ios::binary);
        char signature[2]{};
        file.read(signature, 2);
        if (!file || signature[0] != 'P' || signature[1] != 'K')
            throw std::runtime_error("Failed export overwrote the existing workbook");
        std::cout << "Excel export generation and validation passed\n";
        return 0;
    } catch (std::exception const& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
