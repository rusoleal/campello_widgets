#include <campello_widgets/campello_widgets.hpp>
#include <campello_widgets/macos/run_app.hpp>

#include <string>
#include <sstream>
#include <iomanip>

namespace cw = systems::leal::campello_widgets;

// ---------------------------------------------------------------------------
// TableView Example - Spreadsheet-like demo
// ---------------------------------------------------------------------------

static std::string columnLetter(int col)
{
    std::string result;
    while (col >= 0)
    {
        result = static_cast<char>('A' + (col % 26)) + result;
        col = col / 26 - 1;
    }
    return result;
}

class TableViewApp : public cw::StatelessWidget
{
public:
    cw::WidgetRef build(cw::BuildContext&) const override
    {
        const int num_rows = 1000;
        const int num_cols = 26;

        cw::TextStyle headerStyle{};
        headerStyle.font_family = "Helvetica Neue";
        headerStyle.font_size   = 13.0f;
        headerStyle.font_weight = cw::FontWeight::bold;
        headerStyle.color       = cw::Color::fromRGB(0.3f, 0.3f, 0.3f);

        cw::TextStyle cellStyle{};
        cellStyle.font_family = "Helvetica Neue";
        cellStyle.font_size   = 12.0f;
        cellStyle.color       = cw::Color::fromRGB(0.2f, 0.2f, 0.2f);

        // Create table
        auto table = std::make_shared<cw::TableView>();
        table->extents = {num_rows, num_cols};

        // Configure rows
        table->row_spans.reserve(num_rows);
        for (int r = 0; r < num_rows; ++r)
        {
            cw::TableSpan span;
            span.extent = (r == 0) ? 36.0f : 32.0f;  // Header row taller
            span.pinned = (r == 0);                   // Pin header row
            table->row_spans.push_back(span);
        }

        // Configure columns
        table->column_spans.reserve(num_cols);
        for (int c = 0; c < num_cols; ++c)
        {
            cw::TableSpan span;
            span.extent = (c == 0) ? 60.0f : 100.0f;  // First column narrower
            span.pinned = (c == 0);                    // Pin first column
            table->column_spans.push_back(span);
        }

        // Cell builder
        table->cell_builder = [headerStyle, cellStyle, num_rows, num_cols](
            cw::BuildContext&, int row, int col) -> cw::WidgetRef
        {
            bool is_header_row = (row == 0);
            bool is_header_col = (col == 0);
            bool is_corner = is_header_row && is_header_col;

            // Background color
            cw::Color bg_color;
            if (is_corner)
                bg_color = cw::Color::fromRGB(0.85f, 0.85f, 0.88f);
            else if (is_header_row || is_header_col)
                bg_color = cw::Color::fromRGB(0.92f, 0.92f, 0.95f);
            else if (row % 2 == 0)
                bg_color = cw::Color::white();
            else
                bg_color = cw::Color::fromRGB(0.97f, 0.97f, 0.99f);

            // Cell text
            std::string text;
            if (is_corner)
                text = "";
            else if (is_header_row)
                text = columnLetter(col);
            else if (is_header_col)
                text = std::to_string(row);
            else
                text = std::to_string(row * num_cols + col);

            // Text style
            cw::TextStyle style = (is_header_row || is_header_col) ? headerStyle : cellStyle;

            auto container = std::make_shared<cw::Container>();
            container->color = bg_color;
            container->padding = cw::EdgeInsets::symmetric(8.0f, 0.0f);
            container->child = cw::mw<cw::Center>(cw::mw<cw::Text>(text, style));

                return container;
        };

        table->physics = std::make_shared<cw::BouncingScrollPhysics>();

        // Title bar
        cw::TextStyle titleStyle{};
        titleStyle.font_family = "Helvetica Neue";
        titleStyle.font_size   = 16.0f;
        titleStyle.font_weight = cw::FontWeight::bold;
        titleStyle.color       = cw::Color::fromRGB(0.1f, 0.1f, 0.1f);

        auto title = std::make_shared<cw::Container>();
        title->color = cw::Color::fromRGB(0.95f, 0.95f, 0.98f);
        title->padding = cw::EdgeInsets::symmetric(16.0f, 14.0f);
        title->child = cw::mw<cw::Text>(
            "TableView Example — " + std::to_string(num_rows) + " rows × " +
            std::to_string(num_cols) + " columns",
            titleStyle);

        // Instructions
        cw::TextStyle infoStyle{};
        infoStyle.font_family = "Helvetica Neue";
        infoStyle.font_size   = 12.0f;
        infoStyle.color       = cw::Color::fromRGB(0.5f, 0.5f, 0.5f);

        auto info = std::make_shared<cw::Container>();
        info->color = cw::Color::fromRGB(0.97f, 0.97f, 0.99f);
        info->padding = cw::EdgeInsets::symmetric(16.0f, 8.0f);
        info->child = cw::mw<cw::Text>(
            "Drag to scroll • First row and column are pinned",
            infoStyle);

        auto column = cw::mw<cw::Column>(
            cw::MainAxisAlignment::start,
            cw::CrossAxisAlignment::stretch,
            cw::WidgetList{
                title,
                info,
                cw::mw<cw::Expanded>(table),
            }
        );

        auto bg = std::make_shared<cw::Container>();
        bg->color = cw::Color::fromRGB(1.0f, 1.0f, 1.0f);
        bg->child = column;
        return bg;
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main()
{
    cw::DebugFlags::showDebugBanner        = false;
    cw::DebugFlags::showPerformanceOverlay = true;

    return cw::runApp(
        std::make_shared<TableViewApp>(),
        "campello_widgets — Table View",
        800.0f,
        600.0f);
}
