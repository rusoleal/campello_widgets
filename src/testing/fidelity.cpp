#include <campello_widgets/testing/fidelity.hpp>
#include <campello_widgets/ui/canvas.hpp>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/render_flex.hpp>
#include <campello_widgets/ui/render_padding.hpp>
#include <campello_widgets/ui/render_align.hpp>
#include <campello_widgets/ui/render_sized_box.hpp>
#include <campello_widgets/ui/render_colored_box.hpp>
#include <campello_widgets/ui/paint_context.hpp>
#include <campello_widgets/ui/paint.hpp>
#include <campello_widgets/ui/color.hpp>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <cxxabi.h>

namespace cw = systems::leal::campello_widgets;
namespace cwt = systems::leal::campello_widgets::testing;

// ---------------------------------------------------------------------
// Helper: Type name extraction
// ---------------------------------------------------------------------

static std::string demangleTypeName(const char* name)
{
    int status = 0;
    char* demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
    if (status == 0 && demangled != nullptr) {
        std::string result(demangled);
        free(demangled);
        // Remove namespace prefixes for readability
        const std::string prefix = "systems::leal::campello_widgets::";
        size_t pos = result.find(prefix);
        if (pos != std::string::npos) {
            result = result.substr(pos + prefix.length());
        }
        return result;
    }
    return std::string(name);
}

// ---------------------------------------------------------------------
// RenderNodeSnapshot Implementation
// ---------------------------------------------------------------------

bool cwt::RenderNodeSnapshot::operator==(const RenderNodeSnapshot& other) const noexcept
{
    if (type != other.type) return false;
    if (width != other.width) return false;
    if (height != other.height) return false;
    if (offset_x != other.offset_x) return false;
    if (offset_y != other.offset_y) return false;
    if (constraint_min_w != other.constraint_min_w) return false;
    if (constraint_max_w != other.constraint_max_w) return false;
    if (constraint_min_h != other.constraint_min_h) return false;
    if (constraint_max_h != other.constraint_max_h) return false;
    if (children.size() != other.children.size()) return false;
    if (properties.size() != other.properties.size()) return false;
    
    for (size_t i = 0; i < children.size(); ++i) {
        if (!(children[i] == other.children[i])) return false;
    }
    
    for (size_t i = 0; i < properties.size(); ++i) {
        if (properties[i] != other.properties[i]) return false;
    }
    
    return true;
}

// ---------------------------------------------------------------------
// JSON Helpers
// ---------------------------------------------------------------------

static std::string jsonEscape(const std::string& s)
{
    std::string result;
    for (char c : s) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c;
        }
    }
    return result;
}

static std::string jsonFloat(float f)
{
    if (std::isinf(f)) return f > 0 ? "Infinity" : "-Infinity";
    if (std::isnan(f)) return "NaN";
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << f;
    std::string result = oss.str();
    // Trim trailing zeros
    while (result.size() > 1 && result.back() == '0' && result[result.size()-2] != '.') {
        result.pop_back();
    }
    return result;
}

static std::string indent(int level)
{
    return std::string(level * 2, ' ');
}

// ---------------------------------------------------------------------
// Render Tree Dumping
// ---------------------------------------------------------------------

// Forward declarations
static cwt::RenderNodeSnapshot dumpRenderBoxInternal(
    const cw::RenderBox& box,
    const cw::Offset& offset);

static void dumpFlexChildren(const cw::RenderFlex& flex, cwt::RenderNodeSnapshot& snapshot, const cw::Offset& offset);

cwt::RenderNodeSnapshot cwt::dumpRenderTree(
    const cw::RenderObject& root,
    const cw::Offset& offset)
{
    cwt::RenderNodeSnapshot snapshot;
    snapshot.type = demangleTypeName(typeid(root).name());
    snapshot.width = root.size().width;
    snapshot.height = root.size().height;
    snapshot.offset_x = offset.x;
    snapshot.offset_y = offset.y;
    snapshot.constraint_min_w = root.constraints().min_width;
    snapshot.constraint_max_w = root.constraints().max_width;
    snapshot.constraint_min_h = root.constraints().min_height;
    snapshot.constraint_max_h = root.constraints().max_height;
    
    // Try to cast to RenderBox for child access
    const cw::RenderBox* box = dynamic_cast<const cw::RenderBox*>(&root);
    if (box != nullptr) {
        // Handle RenderFlex specially to get child offsets
        const cw::RenderFlex* flex = dynamic_cast<const cw::RenderFlex*>(&root);
        if (flex != nullptr) {
            dumpFlexChildren(*flex, snapshot, offset);
        } else if (box->child() != nullptr) {
            // For single-child boxes, we need the child offset
            // This is a simplification - in reality we'd need friend access or
            // a virtual method to get the child offset
            auto child_snapshot = cwt::dumpRenderTree(*box->child(), offset);
            snapshot.children.push_back(child_snapshot);
        }
    }
    
    return snapshot;
}

// ---------------------------------------------------------------------
// JSON Serialization
// ---------------------------------------------------------------------

std::string cwt::renderNodeToJson(const RenderNodeSnapshot& node, int indent_level)
{
    std::ostringstream oss;
    std::string ind = indent(indent_level);
    std::string inner = indent(indent_level + 1);
    
    oss << ind << "{\n";
    oss << inner << "\"type\": \"" << jsonEscape(node.type) << "\",\n";
    oss << inner << "\"size\": {\n";
    oss << inner << "  \"width\": " << jsonFloat(node.width) << ",\n";
    oss << inner << "  \"height\": " << jsonFloat(node.height) << "\n";
    oss << inner << "},\n";
    oss << inner << "\"offset\": {\n";
    oss << inner << "  \"x\": " << jsonFloat(node.offset_x) << ",\n";
    oss << inner << "  \"y\": " << jsonFloat(node.offset_y) << "\n";
    oss << inner << "},\n";
    oss << inner << "\"constraints\": {\n";
    oss << inner << "  \"min_width\": " << jsonFloat(node.constraint_min_w) << ",\n";
    oss << inner << "  \"max_width\": " << jsonFloat(node.constraint_max_w) << ",\n";
    oss << inner << "  \"min_height\": " << jsonFloat(node.constraint_min_h) << ",\n";
    oss << inner << "  \"max_height\": " << jsonFloat(node.constraint_max_h) << "\n";
    oss << inner << "}";
    
    if (!node.properties.empty()) {
        oss << ",\n" << inner << "\"properties\": {\n";
        for (size_t i = 0; i < node.properties.size(); ++i) {
            oss << inner << "  \"" << jsonEscape(node.properties[i].first) << "\": \"" 
                << jsonEscape(node.properties[i].second) << "\"";
            if (i < node.properties.size() - 1) oss << ",";
            oss << "\n";
        }
        oss << inner << "}";
    }
    
    if (!node.children.empty()) {
        oss << ",\n" << inner << "\"children\": [\n";
        for (size_t i = 0; i < node.children.size(); ++i) {
            oss << renderNodeToJson(node.children[i], indent_level + 2);
            if (i < node.children.size() - 1) oss << ",";
            oss << "\n";
        }
        oss << inner << "]";
    }
    
    oss << "\n" << ind << "}";
    return oss.str();
}

cwt::RenderNodeSnapshot cwt::renderNodeFromJson(const std::string& json)
{
    // Simple JSON parser - in production, use a proper JSON library
    // This is a placeholder implementation
    (void)json;
    cwt::RenderNodeSnapshot snapshot;
    // TODO: Implement proper JSON parsing
    return snapshot;
}

// ---------------------------------------------------------------------
// Human-Readable Format
// ---------------------------------------------------------------------

std::string cwt::renderNodeToString(const RenderNodeSnapshot& node, int indent_level)
{
    std::ostringstream oss;
    std::string ind(indent_level * 2, ' ');
    
    oss << ind << node.type << "(" << node.width << "×" << node.height << ")";
    oss << " at (" << node.offset_x << ", " << node.offset_y << ")";
    oss << " [" << node.constraint_min_w << "-" << node.constraint_max_w;
    oss << " × " << node.constraint_min_h << "-" << node.constraint_max_h << "]";
    
    if (!node.properties.empty()) {
        oss << " {";
        for (size_t i = 0; i < node.properties.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << node.properties[i].first << "=" << node.properties[i].second;
        }
        oss << "}";
    }
    
    oss << "\n";
    
    for (const auto& child : node.children) {
        oss << renderNodeToString(child, indent_level + 1);
    }
    
    return oss.str();
}

// ---------------------------------------------------------------------
// DrawCommandSnapshot Implementation
// ---------------------------------------------------------------------

bool cwt::DrawCommandSnapshot::operator==(const DrawCommandSnapshot& other) const noexcept
{
    if (type != other.type) return false;
    
    // Compare based on command type
    if (type == "rect") {
        return rect_left == other.rect_left &&
               rect_top == other.rect_top &&
               rect_right == other.rect_right &&
               rect_bottom == other.rect_bottom &&
               paint_red == other.paint_red &&
               paint_green == other.paint_green &&
               paint_blue == other.paint_blue &&
               paint_alpha == other.paint_alpha &&
               paint_style == other.paint_style;
    }
    if (type == "text") {
        return text_content == other.text_content &&
               text_origin_x == other.text_origin_x &&
               text_origin_y == other.text_origin_y;
    }
    if (type == "image") {
        return image_texture_id == other.image_texture_id &&
               image_dst_left == other.image_dst_left &&
               image_dst_top == other.image_dst_top &&
               image_dst_right == other.image_dst_right &&
               image_dst_bottom == other.image_dst_bottom;
    }
    if (type == "push_transform") {
        return transform_matrix == other.transform_matrix;
    }
    if (type == "push_clip") {
        return clip_left == other.clip_left &&
               clip_top == other.clip_top &&
               clip_right == other.clip_right &&
               clip_bottom == other.clip_bottom;
    }
    // pop_clip, pop_transform have no parameters
    return true;
}

// ---------------------------------------------------------------------
// DrawList Serialization
// ---------------------------------------------------------------------

std::vector<cwt::DrawCommandSnapshot> cwt::serializeDrawList(const cw::DrawList& commands)
{
    std::vector<cwt::DrawCommandSnapshot> snapshots;
    snapshots.reserve(commands.size());
    
    for (const auto& cmd : commands) {
        cwt::DrawCommandSnapshot snapshot;
        
        std::visit([&snapshot](const auto& c) {
            using T = std::decay_t<decltype(c)>;
            
            if constexpr (std::is_same_v<T, cw::DrawRectCmd>) {
                snapshot.type = "rect";
                snapshot.rect_left = c.rect.left();
                snapshot.rect_top = c.rect.top();
                snapshot.rect_right = c.rect.right();
                snapshot.rect_bottom = c.rect.bottom();
                snapshot.paint_red = c.paint.color.r;
                snapshot.paint_green = c.paint.color.g;
                snapshot.paint_blue = c.paint.color.b;
                snapshot.paint_alpha = c.paint.color.a;
                snapshot.paint_style = (c.paint.style == cw::PaintStyle::fill) ? "fill" : "stroke";
                snapshot.stroke_width = c.paint.stroke_width;
            }
            else if constexpr (std::is_same_v<T, cw::DrawTextCmd>) {
                snapshot.type = "text";
                snapshot.text_content = c.span.text;
                snapshot.text_style_family = c.span.style.font_family;
                snapshot.text_style_size = c.span.style.font_size;
                snapshot.text_origin_x = c.origin.x;
                snapshot.text_origin_y = c.origin.y;
            }
            else if constexpr (std::is_same_v<T, cw::DrawImageCmd>) {
                snapshot.type = "image";
                // Texture ID is not directly accessible, use pointer value as identifier
                snapshot.image_texture_id = std::to_string(reinterpret_cast<uintptr_t>(c.texture.get()));
                snapshot.image_src_left = c.src_rect.left();
                snapshot.image_src_top = c.src_rect.top();
                snapshot.image_src_right = c.src_rect.right();
                snapshot.image_src_bottom = c.src_rect.bottom();
                snapshot.image_dst_left = c.dst_rect.left();
                snapshot.image_dst_top = c.dst_rect.top();
                snapshot.image_dst_right = c.dst_rect.right();
                snapshot.image_dst_bottom = c.dst_rect.bottom();
                snapshot.image_opacity = c.opacity;
            }
            else if constexpr (std::is_same_v<T, cw::PushClipRectCmd>) {
                snapshot.type = "push_clip";
                snapshot.clip_left = c.rect.left();
                snapshot.clip_top = c.rect.top();
                snapshot.clip_right = c.rect.right();
                snapshot.clip_bottom = c.rect.bottom();
            }
            else if constexpr (std::is_same_v<T, cw::PopClipRectCmd>) {
                snapshot.type = "pop_clip";
            }
            else if constexpr (std::is_same_v<T, cw::PushTransformCmd>) {
                snapshot.type = "push_transform";
                const float* m = c.transform.data;
                snapshot.transform_matrix.assign(m, m + 16);
            }
            else if constexpr (std::is_same_v<T, cw::PopTransformCmd>) {
                snapshot.type = "pop_transform";
            }
        }, cmd);
        
        snapshots.push_back(snapshot);
    }
    
    return snapshots;
}

std::string cwt::drawListToJson(const std::vector<DrawCommandSnapshot>& snapshots)
{
    std::ostringstream oss;
    oss << "[\n";
    
    for (size_t i = 0; i < snapshots.size(); ++i) {
        const auto& s = snapshots[i];
        oss << "  {\n";
        oss << "    \"type\": \"" << jsonEscape(s.type) << "\"";
        
        if (s.type == "rect") {
            oss << ",\n    \"rect\": {\n";
            oss << "      \"left\": " << jsonFloat(s.rect_left) << ",\n";
            oss << "      \"top\": " << jsonFloat(s.rect_top) << ",\n";
            oss << "      \"right\": " << jsonFloat(s.rect_right) << ",\n";
            oss << "      \"bottom\": " << jsonFloat(s.rect_bottom) << "\n";
            oss << "    },\n";
            oss << "    \"paint\": {\n";
            oss << "      \"color\": {\n";
            oss << "        \"r\": " << jsonFloat(s.paint_red) << ",\n";
            oss << "        \"g\": " << jsonFloat(s.paint_green) << ",\n";
            oss << "        \"b\": " << jsonFloat(s.paint_blue) << ",\n";
            oss << "        \"a\": " << jsonFloat(s.paint_alpha) << "\n";
            oss << "      },\n";
            oss << "      \"style\": \"" << s.paint_style << "\",\n";
            oss << "      \"stroke_width\": " << jsonFloat(s.stroke_width) << "\n";
            oss << "    }";
        }
        else if (s.type == "text") {
            oss << ",\n    \"text\": \"" << jsonEscape(s.text_content) << "\",\n";
            oss << "    \"style\": {\n";
            oss << "      \"family\": \"" << jsonEscape(s.text_style_family) << "\",\n";
            oss << "      \"size\": " << jsonFloat(s.text_style_size) << "\n";
            oss << "    },\n";
            oss << "    \"origin\": {\n";
            oss << "      \"x\": " << jsonFloat(s.text_origin_x) << ",\n";
            oss << "      \"y\": " << jsonFloat(s.text_origin_y) << "\n";
            oss << "    }";
        }
        else if (s.type == "image") {
            oss << ",\n    \"texture_id\": \"" << jsonEscape(s.image_texture_id) << "\",\n";
            oss << "    \"src_rect\": {\n";
            oss << "      \"left\": " << jsonFloat(s.image_src_left) << ",\n";
            oss << "      \"top\": " << jsonFloat(s.image_src_top) << ",\n";
            oss << "      \"right\": " << jsonFloat(s.image_src_right) << ",\n";
            oss << "      \"bottom\": " << jsonFloat(s.image_src_bottom) << "\n";
            oss << "    },\n";
            oss << "    \"dst_rect\": {\n";
            oss << "      \"left\": " << jsonFloat(s.image_dst_left) << ",\n";
            oss << "      \"top\": " << jsonFloat(s.image_dst_top) << ",\n";
            oss << "      \"right\": " << jsonFloat(s.image_dst_right) << ",\n";
            oss << "      \"bottom\": " << jsonFloat(s.image_dst_bottom) << "\n";
            oss << "    },\n";
            oss << "    \"opacity\": " << jsonFloat(s.image_opacity);
        }
        else if (s.type == "push_clip") {
            oss << ",\n    \"rect\": {\n";
            oss << "      \"left\": " << jsonFloat(s.clip_left) << ",\n";
            oss << "      \"top\": " << jsonFloat(s.clip_top) << ",\n";
            oss << "      \"right\": " << jsonFloat(s.clip_right) << ",\n";
            oss << "      \"bottom\": " << jsonFloat(s.clip_bottom) << "\n";
            oss << "    }";
        }
        else if (s.type == "push_transform") {
            oss << ",\n    \"matrix\": [\n";
            for (size_t j = 0; j < s.transform_matrix.size(); ++j) {
                oss << "      " << jsonFloat(s.transform_matrix[j]);
                if (j < s.transform_matrix.size() - 1) oss << ",";
                oss << "\n";
            }
            oss << "    ]";
        }
        
        oss << "\n  }";
        if (i < snapshots.size() - 1) oss << ",";
        oss << "\n";
    }
    
    oss << "]";
    return oss.str();
}

std::vector<cwt::DrawCommandSnapshot> cwt::drawListFromJson(const std::string& json)
{
    // Placeholder - in production, use a proper JSON library
    (void)json;
    return {};
}

// ---------------------------------------------------------------------
// Comparison Implementation
// ---------------------------------------------------------------------

void cwt::TreeComparisonResult::addDifference(
    const std::string& path,
    const std::string& expected,
    const std::string& actual)
{
    match = false;
    differences.push_back(path + ": expected '" + expected + "' but got '" + actual + "'");
}

cwt::TreeComparisonResult cwt::compareRenderTrees(
    const RenderNodeSnapshot& expected,
    const RenderNodeSnapshot& actual,
    const std::string& path)
{
    TreeComparisonResult result;
    
    if (expected.type != actual.type) {
        result.addDifference(path + ".type", expected.type, actual.type);
    }
    if (expected.width != actual.width) {
        result.addDifference(path + ".width", std::to_string(expected.width), std::to_string(actual.width));
    }
    if (expected.height != actual.height) {
        result.addDifference(path + ".height", std::to_string(expected.height), std::to_string(actual.height));
    }
    if (expected.offset_x != actual.offset_x) {
        result.addDifference(path + ".offset.x", std::to_string(expected.offset_x), std::to_string(actual.offset_x));
    }
    if (expected.offset_y != actual.offset_y) {
        result.addDifference(path + ".offset.y", std::to_string(expected.offset_y), std::to_string(actual.offset_y));
    }
    
    if (expected.children.size() != actual.children.size()) {
        result.addDifference(
            path + ".children.count",
            std::to_string(expected.children.size()),
            std::to_string(actual.children.size()));
    } else {
        for (size_t i = 0; i < expected.children.size(); ++i) {
            auto child_result = compareRenderTrees(
                expected.children[i],
                actual.children[i],
                path + ".children[" + std::to_string(i) + "]");
            if (!child_result.match) {
                result.match = false;
                result.differences.insert(
                    result.differences.end(),
                    child_result.differences.begin(),
                    child_result.differences.end());
            }
        }
    }
    
    return result;
}

cwt::DrawListComparisonResult cwt::compareDrawLists(
    const std::vector<DrawCommandSnapshot>& expected,
    const std::vector<DrawCommandSnapshot>& actual,
    float tolerance)
{
    DrawListComparisonResult result;
    result.expected_count = expected.size();
    result.actual_count = actual.size();
    
    if (expected.size() != actual.size()) {
        result.match = false;
        result.differences.push_back(
            "Command count mismatch: expected " + std::to_string(expected.size()) +
            " but got " + std::to_string(actual.size()));
    }
    
    size_t count = std::min(expected.size(), actual.size());
    for (size_t i = 0; i < count; ++i) {
        const auto& e = expected[i];
        const auto& a = actual[i];
        
        if (e.type != a.type) {
            result.match = false;
            result.differences.push_back(
                "Command[" + std::to_string(i) + "].type: expected '" + e.type +
                "' but got '" + a.type + "'");
            continue;
        }
        
        // Compare floats with tolerance
        auto float_eq = [tolerance](float x, float y) {
            return std::fabs(x - y) <= tolerance;
        };
        
        if (e.type == "rect") {
            if (!float_eq(e.rect_left, a.rect_left) ||
                !float_eq(e.rect_top, a.rect_top) ||
                !float_eq(e.rect_right, a.rect_right) ||
                !float_eq(e.rect_bottom, a.rect_bottom)) {
                result.match = false;
                result.differences.push_back(
                    "Command[" + std::to_string(i) + "]: rect mismatch");
            }
        }
        else if (e.type == "text") {
            if (e.text_content != a.text_content) {
                result.match = false;
                result.differences.push_back(
                    "Command[" + std::to_string(i) + "].text: expected '" +
                    e.text_content + "' but got '" + a.text_content + "'");
            }
        }
    }
    
    return result;
}

// ---------------------------------------------------------------------
// Golden File Management
// ---------------------------------------------------------------------

std::string cwt::getGoldensDirectory()
{
    // Try to find the goldens directory relative to the executable
    std::filesystem::path paths[] = {
        "tests/goldens",
        "../tests/goldens",
        "../../tests/goldens",
        "../../../tests/goldens",
    };
    
    for (const auto& p : paths) {
        if (std::filesystem::exists(p)) {
            return p.string();
        }
    }
    
    // Default fallback
    return "tests/goldens";
}

std::string cwt::loadGoldenFile(const std::string& name)
{
    std::filesystem::path path = std::filesystem::path(getGoldensDirectory()) / name;
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    return std::string((std::istreambuf_iterator<char>(file)),
                       std::istreambuf_iterator<char>());
}

void cwt::saveGoldenFile(const std::string& name, const std::string& content)
{
    std::filesystem::path dir = getGoldensDirectory();
    std::filesystem::create_directories(dir);
    std::filesystem::path path = dir / name;
    std::ofstream file(path);
    file << content;
}

// ---------------------------------------------------------------------
// FidelitySnapshot Implementation
// ---------------------------------------------------------------------

std::string cwt::FidelitySnapshot::toJson() const
{
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"viewport\": {\n";
    oss << "    \"width\": " << viewport_width << ",\n";
    oss << "    \"height\": " << viewport_height << "\n";
    oss << "  },\n";
    oss << "  \"layout\": " << renderNodeToJson(layout, 1) << ",\n";
    oss << "  \"paint_commands\": " << drawListToJson(paint_commands) << "\n";
    oss << "}";
    return oss.str();
}

cwt::FidelitySnapshot cwt::FidelitySnapshot::fromJson(const std::string& json)
{
    // Placeholder - in production, use a proper JSON library
    (void)json;
    return FidelitySnapshot{};
}

cwt::FidelitySnapshot cwt::captureSnapshot(
    cw::RenderObject& root,
    const cw::BoxConstraints& constraints,
    float viewport_width,
    float viewport_height)
{
    FidelitySnapshot snapshot;
    snapshot.viewport_width = viewport_width;
    snapshot.viewport_height = viewport_height;
    
    // Perform layout
    root.layout(constraints);
    
    // Capture layout tree
    snapshot.layout = dumpRenderTree(root, cw::Offset::zero());
    
    // Capture paint commands
    cw::PaintContext context(viewport_width, viewport_height);
    root.paint(context, cw::Offset::zero());
    snapshot.paint_commands = serializeDrawList(context.commands());
    
    return snapshot;
}

// ---------------------------------------------------------------------
// Flex-specific dumping (needs access to protected members)
// ---------------------------------------------------------------------

// Note: This is a simplified implementation. For full fidelity, you would need
// to either make the testing code a friend of RenderFlex or add public accessors.
static void dumpFlexChildren(const cw::RenderFlex& flex, cwt::RenderNodeSnapshot& snapshot, const cw::Offset& offset)
{
    // Since flex_children_ is private, we can't access it directly here.
    // In a real implementation, you would either:
    // 1. Add friend class declaration in RenderFlex
    // 2. Add public accessor methods for child offsets
    // 3. Use the hit-test infrastructure to walk children
    //
    // For now, we rely on the base class child() which only gives us the first child
    // This is a limitation that should be addressed for complete fidelity testing
    (void)flex;
    (void)snapshot;
    (void)offset;
}
