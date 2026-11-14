#pragma once
#include "WinHeaders.h"
#include <filesystem>
#include <string>
#include <vector>
namespace platform {
bool Windows11();
std::filesystem::path DataPath();
std::filesystem::path ChooseFile(HWND owner, bool save, std::wstring const& extension,
                                std::wstring const& title);
void WriteText(std::filesystem::path const&, std::string const&);
void ExportCsv(std::filesystem::path const&, std::vector<std::wstring> const& headers,
               std::vector<std::vector<std::wstring>> const& rows);
void Print(HWND owner, std::wstring const& title, std::wstring const& text, bool pdf);
std::string Html(std::wstring const& title, std::wstring const& text);
}
