#pragma once

#include <campello_widgets/ui/render_object.hpp>
#include <campello_widgets/ui/draw_command.hpp>
#include <string>
#include <vector>
#include <memory>
#include <sstream>

namespace systems::leal::campello_widgets::testing
{

    // ---------------------------------------------------------------------
    // Layout Tree Dumping (Layer 2 Fidelity)
    // ---------------------------------------------------------------------

    /**
     * @brief Serialized representation of a RenderObject node for comparison.
     *
     * Captures the type, size, position, and constraints of each render object
     * in the tree. This enables structural fidelity testing against Flutter's
     * render tree output.
     */
    struct RenderNodeSnapshot
    {
        std::string type;                       // e.g., "RenderFlex", "RenderPadding"
        std::string id;                         // Optional identifier for debugging
        float width = 0.0f;
        float height = 0.0f;
        float offset_x = 0.0f;
        float offset_y = 0.0f;
        float constraint_min_w = 0.0f;
        float constraint_max_w = 0.0f;
        float constraint_min_h = 0.0f;
        float constraint_max_h = 0.0f;
        std::vector<RenderNodeSnapshot> children;
        
        // Additional properties specific to the render object type
        std::vector<std::pair<std::string, std::string>> properties;

        bool operator==(const RenderNodeSnapshot& other) const noexcept;
        bool operator!=(const RenderNodeSnapshot& other) const noexcept { return !(*this == other); }
    };

    /**
     * @brief Dumps a RenderObject tree to a serializable snapshot.
     *
     * Traverses the render tree starting from `root`, capturing the size,
     * position, and constraints of each node. This can be compared against
     * Flutter's render tree output to verify layout fidelity.
     *
     * @param root The root RenderObject to dump.
     * @param offset The initial offset (typically zero).
     * @return A snapshot of the entire render tree.
     */
    RenderNodeSnapshot dumpRenderTree(
        const RenderObject& root,
        const Offset& offset = Offset::zero());

    /**
     * @brief Serializes a RenderNodeSnapshot to JSON format.
     */
    std::string renderNodeToJson(const RenderNodeSnapshot& node, int indent = 0);

    /**
     * @brief Deserializes a RenderNodeSnapshot from JSON format.
     */
    RenderNodeSnapshot renderNodeFromJson(const std::string& json);

    /**
     * @brief Formats a RenderNodeSnapshot as a human-readable tree.
     */
    std::string renderNodeToString(const RenderNodeSnapshot& node, int indent = 0);

    // ---------------------------------------------------------------------
    // DrawList Serialization (Layer 3 Fidelity)
    // ---------------------------------------------------------------------

    /**
     * @brief Serialized representation of a DrawCommand for comparison.
     *
     * Captures all parameters of each draw command emitted by Canvas.
     * This enables paint fidelity testing without GPU dependencies.
     */
    struct DrawCommandSnapshot
    {
        std::string type;  // "rect", "text", "image", "push_clip", "pop_clip", etc.
        
        // Rect commands
        float rect_left = 0.0f;
        float rect_top = 0.0f;
        float rect_right = 0.0f;
        float rect_bottom = 0.0f;
        
        // Paint properties (for rect)
        float paint_red = 0.0f;
        float paint_green = 0.0f;
        float paint_blue = 0.0f;
        float paint_alpha = 1.0f;
        std::string paint_style;  // "fill" or "stroke"
        float stroke_width = 0.0f;
        
        // Text commands
        std::string text_content;
        std::string text_style_family;
        float text_style_size = 0.0f;
        float text_origin_x = 0.0f;
        float text_origin_y = 0.0f;
        
        // Image commands
        std::string image_texture_id;  // Identifier for comparison
        float image_src_left = 0.0f;
        float image_src_top = 0.0f;
        float image_src_right = 1.0f;
        float image_src_bottom = 1.0f;
        float image_dst_left = 0.0f;
        float image_dst_top = 0.0f;
        float image_dst_right = 0.0f;
        float image_dst_bottom = 0.0f;
        float image_opacity = 1.0f;
        
        // Transform commands
        std::vector<float> transform_matrix;  // 16 floats for Matrix4
        
        // Clip commands
        float clip_left = 0.0f;
        float clip_top = 0.0f;
        float clip_right = 0.0f;
        float clip_bottom = 0.0f;

        bool operator==(const DrawCommandSnapshot& other) const noexcept;
        bool operator!=(const DrawCommandSnapshot& other) const noexcept { return !(*this == other); }
    };

    /**
     * @brief Serializes a DrawList to a vector of snapshots.
     */
    std::vector<DrawCommandSnapshot> serializeDrawList(const DrawList& commands);

    /**
     * @brief Converts a DrawList to JSON format.
     */
    std::string drawListToJson(const std::vector<DrawCommandSnapshot>& snapshots);

    /**
     * @brief Parses a DrawList from JSON format.
     */
    std::vector<DrawCommandSnapshot> drawListFromJson(const std::string& json);

    // ---------------------------------------------------------------------
    // Comparison & Diffing
    // ---------------------------------------------------------------------

    /**
     * @brief Result of comparing two render trees.
     */
    struct TreeComparisonResult
    {
        bool match = true;
        std::vector<std::string> differences;
        
        void addDifference(const std::string& path, const std::string& expected, const std::string& actual);
    };

    /**
     * @brief Deep comparison of two render trees with detailed diff output.
     */
    TreeComparisonResult compareRenderTrees(
        const RenderNodeSnapshot& expected,
        const RenderNodeSnapshot& actual,
        const std::string& path = "root");

    /**
     * @brief Result of comparing two draw command lists.
     */
    struct DrawListComparisonResult
    {
        bool match = true;
        std::vector<std::string> differences;
        size_t expected_count = 0;
        size_t actual_count = 0;
    };

    /**
     * @brief Comparison of two draw command lists.
     *
     * Commands are compared in order. Tolerance can be applied to float values.
     */
    DrawListComparisonResult compareDrawLists(
        const std::vector<DrawCommandSnapshot>& expected,
        const std::vector<DrawCommandSnapshot>& actual,
        float tolerance = 0.001f);

    // ---------------------------------------------------------------------
    // Golden File Management
    // ---------------------------------------------------------------------

    /**
     * @brief Loads a golden file from the tests/goldens directory.
     */
    std::string loadGoldenFile(const std::string& name);

    /**
     * @brief Saves content to a golden file (for generating new goldens).
     */
    void saveGoldenFile(const std::string& name, const std::string& content);

    /**
     * @brief Returns the path to the goldens directory.
     */
    std::string getGoldensDirectory();

    // ---------------------------------------------------------------------
    // Test Harness Helpers
    // ---------------------------------------------------------------------

    /**
     * @brief Captures a complete fidelity snapshot of a render tree.
     *
     * Combines layout dump and draw command capture in a single call.
     */
    struct FidelitySnapshot
    {
        RenderNodeSnapshot layout;
        std::vector<DrawCommandSnapshot> paint_commands;
        float viewport_width = 0.0f;
        float viewport_height = 0.0f;
        
        std::string toJson() const;
        static FidelitySnapshot fromJson(const std::string& json);
    };

    /**
     * @brief Captures a fidelity snapshot from a RenderObject tree.
     *
     * This performs layout (if needed), dumps the render tree, and captures
     * the paint commands that would be issued.
     */
    FidelitySnapshot captureSnapshot(
        RenderObject& root,
        const BoxConstraints& constraints,
        float viewport_width = 400.0f,
        float viewport_height = 600.0f);

} // namespace systems::leal::campello_widgets::testing
