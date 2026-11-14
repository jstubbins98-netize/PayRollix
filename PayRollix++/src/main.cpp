#include "WinHeaders.h"
#include "AppWindow.h"
#include "Platform.h"
#include <objbase.h>
#include <QApplication>
#include <QMessageBox>
#include <exception>

int main(int argc, char* argv[]) {
    // Check the real Windows version before opening Qt or any payroll database.
    if (!platform::Windows11()) {
        MessageBoxW(nullptr, L"Payrollix++ requires Windows 11 (build 22000 or newer), client edition. Windows 10 and Windows Server are not supported.",
                    L"Unsupported operating system", MB_OK | MB_ICONERROR);
        return ERROR_OLD_WIN_VERSION;
    }
    const auto result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (FAILED(result)) {
        MessageBoxW(nullptr, L"Windows could not initialize desktop services.", L"Payrollix++", MB_OK | MB_ICONERROR);
        return 1;
    }
    struct ComApartment { ~ComApartment() { CoUninitialize(); } } apartment;
    QApplication application(argc, argv);
    application.setApplicationName("Payrollix++");
    application.setOrganizationName("Payrollix");
    try {
        AppWindow window;
        window.show();
        return application.exec();
    } catch (std::exception const& error) {
        QMessageBox::critical(nullptr, "Payrollix++ startup failed", QString::fromUtf8(error.what()));
        return 1;
    }
}
