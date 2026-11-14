#pragma once
#include "WinHeaders.h"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Microsoft.UI.Text.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>

namespace ui
{
    using namespace winrt;
    using namespace winrt::Microsoft::UI::Xaml;
    using namespace winrt::Microsoft::UI::Xaml::Controls;

    // The application owns presentation of errors (for example, a status bar or
    // an InfoBar). Keep this inline so every translation unit shares one handler.
    inline std::function<void(std::wstring const&)> OnError{};

    namespace detail
    {
        inline void ReportError(std::wstring const& message) noexcept
        {
            try
            {
                if (OnError)
                {
                    OnError(message);
                }
            }
            catch (...)
            {
                // Error reporting must never allow an exception to escape a UI event.
            }
        }

        inline void ReportError(std::exception const& error) noexcept
        {
            try
            {
                auto message = winrt::to_hstring(error.what());
                ReportError(std::wstring{ message.c_str() });
            }
            catch (...)
            {
                ReportError(L"An unexpected error occurred.");
            }
        }

        inline double ColumnWidth(
            std::wstring const& header,
            std::vector<std::vector<std::wstring>> const& rows,
            size_t column)
        {
            size_t longest = header.size();
            for (auto const& row : rows)
            {
                if (column < row.size())
                {
                    longest = (std::max)(longest, row[column].size());
                }
            }

            // A readable minimum plus content-aware widths keep data aligned while
            // allowing the ListView's horizontal scroller to handle wide records.
            return (std::clamp)(static_cast<double>(longest) * 8.2 + 32.0, 132.0, 1100.0);
        }

        inline Grid MakeTableRow(
            std::vector<std::wstring> const& values,
            std::vector<double> const& widths,
            bool isHeader)
        {
            Grid row;
            row.VerticalAlignment(VerticalAlignment::Center);

            double totalWidth = 0.0;
            for (auto width : widths)
            {
                ColumnDefinition column;
                column.Width(GridLength{ width, GridUnitType::Pixel });
                row.ColumnDefinitions().Append(column);
                totalWidth += width;
            }
            row.Width(totalWidth);

            for (uint32_t index = 0; index < widths.size(); ++index)
            {
                TextBlock cell;
                cell.Text(index < values.size() ? winrt::hstring{ values[index] } : winrt::hstring{});
                cell.Margin(Thickness{ 12.0, 9.0, 12.0, 9.0 });
                cell.VerticalAlignment(VerticalAlignment::Center);
                cell.TextWrapping(TextWrapping::NoWrap);
                cell.FontSize(13.0);
                if (isHeader)
                {
                    cell.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
                }
                Grid::SetColumn(cell, index);
                row.Children().Append(cell);
            }
            return row;
        }
    }

    inline StackPanel Stack()
    {
        StackPanel panel;
        panel.Orientation(Orientation::Vertical);
        panel.Spacing(12.0);
        return panel;
    }

    inline StackPanel Row()
    {
        StackPanel panel;
        panel.Orientation(Orientation::Horizontal);
        panel.Spacing(10.0);
        panel.VerticalAlignment(VerticalAlignment::Center);
        return panel;
    }

    inline TextBlock Text(std::wstring const& value, double size = 14)
    {
        TextBlock text;
        text.Text(winrt::hstring{ value });
        text.FontSize(size);
        text.TextWrapping(TextWrapping::Wrap);
        return text;
    }

    inline TextBox Input(
        std::wstring const& label,
        std::wstring const& value = L"",
        bool multiline = false)
    {
        TextBox input;
        input.Header(winrt::box_value(winrt::hstring{ label }));
        input.Text(winrt::hstring{ value });
        input.PlaceholderText(winrt::hstring{ label });
        input.MinWidth(220.0);
        if (multiline)
        {
            input.AcceptsReturn(true);
            input.TextWrapping(TextWrapping::Wrap);
            input.MinHeight(112.0);
        }
        return input;
    }

    inline NumberBox Number(std::wstring const& label, double value = 0)
    {
        NumberBox number;
        number.Header(winrt::box_value(winrt::hstring{ label }));
        number.Value(value);
        number.Minimum(0.0);
        number.Maximum(1'000'000'000.0);
        number.SmallChange(1.0);
        number.LargeChange(10.0);
        number.SpinButtonPlacement(NumberBoxSpinButtonPlacement::Inline);
        number.ValidationMode(NumberBoxValidationMode::InvalidInputOverwritten);
        number.MinWidth(180.0);
        return number;
    }

    inline ComboBox Choice(
        std::wstring const& label,
        std::vector<std::wstring> const& options,
        int selected = 0)
    {
        ComboBox choice;
        choice.Header(winrt::box_value(winrt::hstring{ label }));
        choice.MinWidth(220.0);
        for (auto const& option : options)
        {
            choice.Items().Append(winrt::box_value(winrt::hstring{ option }));
        }

        if (options.empty())
        {
            choice.SelectedIndex(-1);
        }
        else
        {
            choice.SelectedIndex((std::clamp)(selected, 0, static_cast<int>(options.size()) - 1));
        }
        return choice;
    }

    inline Button Action(
        std::wstring const& label,
        std::function<void()> callback,
        bool primary = false)
    {
        Button button;
        button.Content(winrt::box_value(winrt::hstring{ label }));
        button.MinWidth(96.0);
        button.Click([callback = std::move(callback)](auto const&, auto const&)
        {
            try
            {
                if (callback)
                {
                    callback();
                }
            }
            catch (winrt::hresult_error const& error)
            {
                detail::ReportError(std::wstring{ error.message().c_str() });
            }
            catch (std::exception const& error)
            {
                detail::ReportError(error);
            }
            catch (...)
            {
                detail::ReportError(L"An unexpected error occurred.");
            }
        });

        // Prefer the system-provided accent style; if a host app has not merged
        // that resource, retain the standard accessible WinUI button appearance.
        if (primary)
        {
            try
            {
                auto resources = Application::Current().Resources();
                auto style = resources.Lookup(winrt::box_value(L"AccentButtonStyle")).as<Style>();
                button.Style(style);
            }
            catch (...)
            {
            }
        }
        return button;
    }

    inline void Heading(
        StackPanel const& panel,
        std::wstring const& title,
        std::wstring const& description = L"")
    {
        TextBlock heading;
        heading.Text(winrt::hstring{ title });
        heading.FontSize(24.0);
        heading.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        heading.TextWrapping(TextWrapping::Wrap);
        panel.Children().Append(heading);

        if (!description.empty())
        {
            TextBlock detail;
            detail.Text(winrt::hstring{ description });
            detail.FontSize(14.0);
            detail.Opacity(0.78);
            detail.TextWrapping(TextWrapping::Wrap);
            panel.Children().Append(detail);
        }
    }

    inline ScrollViewer Scroll(UIElement const& content)
    {
        ScrollViewer viewer;
        viewer.Content(content);
        viewer.HorizontalScrollMode(ScrollMode::Auto);
        viewer.VerticalScrollMode(ScrollMode::Auto);
        viewer.HorizontalScrollBarVisibility(ScrollBarVisibility::Auto);
        viewer.VerticalScrollBarVisibility(ScrollBarVisibility::Auto);
        viewer.ZoomMode(ZoomMode::Disabled);
        return viewer;
    }

    inline ListView Table(
        std::vector<std::wstring> const& headers,
        std::vector<std::vector<std::wstring>> const& rows)
    {
        ListView table;
        table.SelectionMode(ListViewSelectionMode::Single);
        table.IsItemClickEnabled(false);

        auto columnCount = headers.size();
        for (auto const& row : rows)
        {
            columnCount = (std::max)(columnCount, row.size());
        }

        std::vector<std::wstring> normalizedHeaders = headers;
        normalizedHeaders.resize(columnCount);
        std::vector<double> widths;
        widths.reserve(columnCount);
        for (size_t column = 0; column < columnCount; ++column)
        {
            widths.push_back(detail::ColumnWidth(normalizedHeaders[column], rows, column));
        }

        // The header is separate from Items, preserving SelectedIndex == input row.
        if (columnCount > 0)
        {
            auto header = detail::MakeTableRow(normalizedHeaders, widths, true);
            table.Header(header);
        }

        for (auto const& values : rows)
        {
            auto item = detail::MakeTableRow(values, widths, false);
            table.Items().Append(item);
        }

        ScrollViewer::SetHorizontalScrollMode(table, ScrollMode::Enabled);
        ScrollViewer::SetVerticalScrollMode(table, ScrollMode::Enabled);
        ScrollViewer::SetHorizontalScrollBarVisibility(table, ScrollBarVisibility::Auto);
        ScrollViewer::SetVerticalScrollBarVisibility(table, ScrollBarVisibility::Auto);
        return table;
    }

    inline std::wstring Value(TextBox const& input)
    {
        return input.Text().c_str();
    }

    inline std::wstring Selected(ComboBox const& choice)
    {
        auto selected = choice.SelectedItem();
        if (!selected)
        {
            return {};
        }
        try
        {
            return winrt::unbox_value<winrt::hstring>(selected).c_str();
        }
        catch (...)
        {
            return {};
        }
    }
}
