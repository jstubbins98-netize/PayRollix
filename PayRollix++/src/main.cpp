#include "WinHeaders.h"
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Windows.UI.Xaml.Interop.h>
#include "AppWindow.h"
#include "Platform.h"

using namespace winrt;
using namespace Microsoft::UI::Xaml;
using namespace Microsoft::UI::Xaml::Markup;
using namespace Microsoft::UI::Xaml::XamlTypeInfo;

// Programmatic WinUI controls still require the framework's XAML metadata provider.
struct PayrollApplication : ApplicationT<PayrollApplication, IXamlMetadataProvider> {
    std::unique_ptr<AppWindow> window;
    XamlControlsXamlMetaDataProvider provider;
    void OnLaunched(LaunchActivatedEventArgs const&) {
        try {
            Resources().MergedDictionaries().Append(Controls::XamlControlsResources());
            window = std::make_unique<AppWindow>();
            window->Activate();
        } catch (hresult_error const& error) {
            MessageBoxW(nullptr,error.message().c_str(),L"Payrollix++ startup failed",MB_OK|MB_ICONERROR);
            Exit();
        } catch (std::exception const& error) {
            MessageBoxW(nullptr,to_hstring(error.what()).c_str(),L"Payrollix++ startup failed",MB_OK|MB_ICONERROR);
            Exit();
        }
    }
    IXamlType GetXamlType(Windows::UI::Xaml::Interop::TypeName const& type) { return provider.GetXamlType(type); }
    IXamlType GetXamlType(hstring const& name) { return provider.GetXamlType(name); }
    com_array<XmlnsDefinition> GetXmlnsDefinitions() { return provider.GetXmlnsDefinitions(); }
};
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    if (!platform::Windows11()) {
        MessageBoxW(nullptr,L"Payrollix++ requires Windows 11 (build 22000 or newer), client edition. Windows 10 and Windows Server are not supported.",
                    L"Unsupported operating system",MB_OK|MB_ICONERROR);
        return ERROR_OLD_WIN_VERSION;
    }
    try {
        init_apartment(apartment_type::single_threaded);
        Application::Start([](auto const&) { make<PayrollApplication>(); });
        return 0;
    } catch (hresult_error const& error) {
        MessageBoxW(nullptr,error.message().c_str(),L"Payrollix++ failed",MB_OK|MB_ICONERROR);
    } catch (std::exception const& error) {
        MessageBoxW(nullptr,to_hstring(error.what()).c_str(),L"Payrollix++ failed",MB_OK|MB_ICONERROR);
    }
    return 1;
}
