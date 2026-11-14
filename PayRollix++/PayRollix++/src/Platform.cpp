#include "Platform.h"
#include "WindowsVersion.h"
#include <winternl.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <commdlg.h>
#include <wrl/client.h>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>

namespace platform {
namespace {
void Check(HRESULT result) {
    if (FAILED(result)) throw std::runtime_error("Windows desktop operation failed (HRESULT " +
                                                std::to_string(static_cast<unsigned long>(result)) + ").");
}
std::string Utf8(std::wstring const& text) {
    if (text.empty()) return {};
    int count = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (!count) throw std::runtime_error("Text could not be converted to UTF-8.");
    std::string result(static_cast<size_t>(count), '\0');
    if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text.data(),
        static_cast<int>(text.size()), result.data(), count, nullptr, nullptr))
        throw std::runtime_error("Text could not be converted to UTF-8.");
    return result;
}
}
bool Windows11() {
    using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    auto module = GetModuleHandleW(L"ntdll.dll");
    auto fn = module ? reinterpret_cast<RtlGetVersionFn>(GetProcAddress(module, "RtlGetVersion")) : nullptr;
    OSVERSIONINFOEXW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (!fn || fn(reinterpret_cast<PRTL_OSVERSIONINFOW>(&version)) != 0) return false;
    // Windows 11 reports NT 10.0. Reject Windows 10, Server and future major releases.
    return SupportedWindows(version.dwMajorVersion, version.dwMinorVersion,
                            version.dwBuildNumber, version.wProductType);
}
std::filesystem::path DataPath() {
    PWSTR raw = nullptr;
    Check(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &raw));
    std::filesystem::path directory(raw);
    CoTaskMemFree(raw);
    directory /= L"Payrollix";
    std::filesystem::create_directories(directory);
    return directory / L"payroll.db";
}
std::filesystem::path ChooseFile(HWND owner, bool save, std::wstring const& ext, std::wstring const& title) {
    Microsoft::WRL::ComPtr<IFileDialog> dialog;
    if (save) Check(CoCreateInstance(CLSID_FileSaveDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.GetAddressOf())));
    else Check(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dialog.GetAddressOf())));
    dialog->SetTitle(title.c_str());
    auto pattern = L"*." + ext;
    COMDLG_FILTERSPEC filter[] = {{pattern.c_str(), pattern.c_str()}};
    Check(dialog->SetFileTypes(1, filter));
    DWORD options = 0;
    Check(dialog->GetOptions(&options));
    Check(dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_NOCHANGEDIR |
        (save ? FOS_OVERWRITEPROMPT : FOS_FILEMUSTEXIST)));
    if (save) dialog->SetDefaultExtension(ext.c_str());
    HRESULT result = dialog->Show(owner);
    if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
    Check(result);
    Microsoft::WRL::ComPtr<IShellItem> item;
    Check(dialog->GetResult(item.GetAddressOf()));
    PWSTR filename = nullptr;
    Check(item->GetDisplayName(SIGDN_FILESYSPATH, &filename));
    std::filesystem::path path(filename);
    CoTaskMemFree(filename);
    if (save && std::filesystem::exists(path) && std::filesystem::equivalent(path,DataPath()))
        throw std::runtime_error("The live payroll database cannot be overwritten by an export. Choose another file.");
    return path;
}
void WriteText(std::filesystem::path const& path, std::string const& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open the output file. Check the folder permissions.");
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    out.close();
    if (!out) throw std::runtime_error("The output file could not be fully written.");
}
void ExportCsv(std::filesystem::path const& path, std::vector<std::wstring> const& headers,
               std::vector<std::vector<std::wstring>> const& rows) {
    std::string output = "\xEF\xBB\xBF";
    auto line = [&](std::vector<std::wstring> const& cells) {
        bool first = true;
        for (auto const& cell : cells) {
            if (!first) output += ',';
            first = false;
            auto text = Utf8(cell);
            // Keep exported names/notes from becoming spreadsheet formulas.
            auto begin = text.find_first_not_of(" \t\r\n");
            if ((!text.empty() && (text[0] == '\t' || text[0] == '\r' || text[0] == '\n')) ||
                (begin != std::string::npos && std::string("=+-@").find(text[begin]) != std::string::npos))
                text.insert(text.begin(), '\'');
            output += '"';
            for (char c : text) { if (c == '"') output += '"'; output += c; }
            output += '"';
        }
        output += "\r\n";
    };
    line(headers);
    for (auto const& row : rows) line(row);
    WriteText(path, output);
}
std::string Html(std::wstring const& title, std::wstring const& text) {
    auto escape = [](std::wstring const& input) {
        std::string result;
        for (char c : Utf8(input)) {
            switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            default: result += c;
            }
        }
        return result;
    };
    return "<!doctype html><html lang=\"en\"><meta charset=\"utf-8\"><title>" + escape(title) +
        "</title><style>body{font:16px 'Segoe UI',sans-serif;margin:40px}pre{font:inherit;white-space:pre-wrap;"
        "line-height:1.7}h1{color:#165d73}</style><h1>" + escape(title) + "</h1><pre>" + escape(text) + "</pre></html>";
}
void Print(HWND owner, std::wstring const& title, std::wstring const& text, bool pdf) {
    HDC dc = nullptr;
    std::filesystem::path file;
    if (pdf) {
        file = ChooseFile(owner, true, L"pdf", L"Export payslip to PDF");
        if (file.empty()) return;
        dc = CreateDCW(L"WINSPOOL", L"Microsoft Print to PDF", nullptr, nullptr);
        if (!dc) throw std::runtime_error("Enable Microsoft Print to PDF in Windows Features, or export HTML instead.");
    } else {
        PRINTDLGW dialog{sizeof(dialog)};
        dialog.hwndOwner = owner;
        dialog.Flags = PD_RETURNDC | PD_NOPAGENUMS | PD_NOSELECTION | PD_USEDEVMODECOPIESANDCOLLATE;
        if (!PrintDlgW(&dialog)) {
            if (dialog.hDevMode) GlobalFree(dialog.hDevMode);
            if (dialog.hDevNames) GlobalFree(dialog.hDevNames);
            auto error = CommDlgExtendedError();
            if (error) throw std::runtime_error("Windows could not open the print dialog.");
            return;
        }
        dc = dialog.hDC;
        if (dialog.hDevMode) GlobalFree(dialog.hDevMode);
        if (dialog.hDevNames) GlobalFree(dialog.hDevNames);
    }
    struct DC { HDC h; ~DC(){ DeleteDC(h); } } holder{dc};
    DOCINFOW document{sizeof(document)};
    document.lpszDocName = title.c_str();
    document.lpszOutput = pdf ? file.c_str() : nullptr;
    if (StartDocW(dc, &document) <= 0) throw std::runtime_error("Windows could not start the print job.");
    HFONT font = CreateFontW(-MulDiv(11, GetDeviceCaps(dc,LOGPIXELSY),72),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,
                             DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    if (!font) { AbortDoc(dc); throw std::runtime_error("Cannot create printer font."); }
    auto old = SelectObject(dc, font);
    try {
        int margin = GetDeviceCaps(dc,LOGPIXELSX)/2;
        int bottom = GetDeviceCaps(dc,VERTRES)-margin;
        int width = GetDeviceCaps(dc,HORZRES)-2*margin;
        TEXTMETRICW metrics{};
        GetTextMetricsW(dc, &metrics);
        int height = metrics.tmHeight + metrics.tmExternalLeading + 8;
        if (StartPage(dc) <= 0) throw std::runtime_error("Could not start a printed page.");
        int y = margin;
        std::wistringstream lines(title + L"\n\n" + text);
        std::wstring line;
        while (std::getline(lines,line)) {
            do {
                int count = static_cast<int>(line.size()), fitting = 0;
                SIZE size{};
                if (count && !GetTextExtentExPointW(dc,line.c_str(),count,width,&fitting,nullptr,&size))
                    throw std::runtime_error("Cannot lay out printed text.");
                if (!count) fitting = 0;
                else if (!fitting) fitting = 1;
                if (y + height > bottom) {
                    if (EndPage(dc) <= 0 || StartPage(dc) <= 0) throw std::runtime_error("Cannot print next page.");
                    y = margin;
                }
                if (fitting && !TextOutW(dc,margin,y,line.c_str(),fitting))
                    throw std::runtime_error("Cannot render printed text.");
                y += height;
                line.erase(0, static_cast<size_t>(fitting));
            } while (!line.empty());
        }
        if (EndPage(dc) <= 0 || EndDoc(dc) <= 0) throw std::runtime_error("The print job did not complete.");
    } catch (...) {
        AbortDoc(dc);
        SelectObject(dc,old);
        DeleteObject(font);
        throw;
    }
    SelectObject(dc,old);
    DeleteObject(font);
}
}
