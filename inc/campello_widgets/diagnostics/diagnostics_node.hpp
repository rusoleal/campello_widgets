#pragma once

#include <campello_widgets/diagnostics/diagnostic_property.hpp>

#include <memory>
#include <string>
#include <sstream>
#include <vector>

namespace systems::leal::campello_widgets
{

    enum class DiagnosticsTreeStyle
    {
        sparse,      // Normal tree, one node per line
        offstage,    // Offstage children shown in brackets
        dense,       // Compact single-line representation
        transition,  // Animation transition nodes
    };

    /**
     * @brief A node in a diagnostic tree.
     *
     * Each node has a name (e.g. "Container"), an optional description,
     * a list of typed properties, and zero or more child nodes.
     *
     * This is the C++ equivalent of Flutter's DiagnosticsNode.
     */
    class DiagnosticsNode
    {
    public:
        std::string name;
        std::string description;                 // Extra description text
        DiagnosticsTreeStyle style = DiagnosticsTreeStyle::sparse;
        DiagnosticLevel level = DiagnosticLevel::info;
        bool allowWrap = false;
        bool showName = true;
        bool showSeparator = true;

        DiagnosticPropertyList properties;
        std::vector<std::shared_ptr<DiagnosticsNode>> children;

        DiagnosticsNode() = default;
        explicit DiagnosticsNode(std::string name_)
            : name(std::move(name_)) {}
        DiagnosticsNode(std::string name_, std::string description_)
            : name(std::move(name_)), description(std::move(description_)) {}

        // ------------------------------------------------------------------
        // String output
        // ------------------------------------------------------------------

        std::string toString() const
        {
            std::ostringstream oss;
            if (showName && !name.empty())
            {
                oss << name;
                if (showSeparator) oss << " ";
            }
            if (!description.empty())
                oss << description;
            else if (!properties.empty())
            {
                bool first = true;
                for (const auto& p : properties)
                {
                    if (!p->isInteresting()) continue;
                    if (!first) oss << ", ";
                    first = false;
                    oss << p->toString();
                }
            }
            return oss.str();
        }

        std::string toStringDeep(const std::string& prefixLineOne = "",
                                 const std::string& prefixOtherLines = "") const
        {
            std::ostringstream oss;
            toStringDeepImpl(oss, prefixLineOne, prefixOtherLines, "");
            return oss.str();
        }

        // ------------------------------------------------------------------
        // JSON-like output (simple, no external dependency)
        // ------------------------------------------------------------------

        std::string toJson() const
        {
            std::ostringstream oss;
            toJsonImpl(oss, 0);
            return oss.str();
        }

    private:
        void toStringDeepImpl(std::ostringstream& oss,
                              std::string prefixLineOne,
                              std::string prefixOtherLines,
                              std::string childPrefix) const
        {
            oss << prefixLineOne << toString();

            if (!children.empty())
            {
                const std::string childLinePrefix = prefixOtherLines + childPrefix;
                const std::string lastChildPrefix = "   ";   // "   "
                const std::string midChildPrefix  = "\u2502  "; // "│  "
                const std::string branchMid       = "\u251C\u2500 "; // "├─ "
                const std::string branchLast      = "\u2514\u2500 "; // "└─ "

                for (size_t i = 0; i < children.size(); ++i)
                {
                    const bool isLast = (i + 1 == children.size());
                    const std::string nextPrefixLineOne = childLinePrefix + (isLast ? branchLast : branchMid);
                    const std::string nextPrefixOther   = childLinePrefix + (isLast ? lastChildPrefix : midChildPrefix);

                    oss << "\n";
                    children[i]->toStringDeepImpl(oss, nextPrefixLineOne, nextPrefixOther, childPrefix);
                }
            }
        }

        void toJsonImpl(std::ostringstream& oss, int indent) const
        {
            auto doIndent = [&](int extra = 0) {
                for (int i = 0; i < indent + extra; ++i) oss << "  ";
            };

            oss << "{\n";
            doIndent(1); oss << "\"name\": \"" << escapeJson(name) << "\"";
            if (!description.empty()) {
                oss << ",\n"; doIndent(1); oss << "\"description\": \"" << escapeJson(description) << "\"";
            }

            if (!properties.empty()) {
                oss << ",\n"; doIndent(1); oss << "\"properties\": [\n";
                bool first = true;
                for (const auto& p : properties) {
                    if (!p->isInteresting()) continue;
                    if (!first) oss << ",\n";
                    first = false;
                    doIndent(2); oss << "{\"name\": \"" << escapeJson(p->name) << "\", ";
                    oss << "\"value\": \"" << escapeJson(p->valueToString()) << "\"}";
                }
                oss << "\n"; doIndent(1); oss << "]";
            }

            if (!children.empty()) {
                oss << ",\n"; doIndent(1); oss << "\"children\": [\n";
                for (size_t i = 0; i < children.size(); ++i) {
                    if (i > 0) oss << ",\n";
                    children[i]->toJsonImpl(oss, indent + 2);
                }
                oss << "\n"; doIndent(1); oss << "]";
            }

            oss << "\n";
            doIndent(); oss << "}";
        }

        static std::string escapeJson(std::string s)
        {
            std::string out;
            out.reserve(s.size());
            for (char c : s) {
                switch (c) {
                    case '"': out += "\\\""; break;
                    case '\\': out += "\\\\"; break;
                    case '\b': out += "\\b"; break;
                    case '\f': out += "\\f"; break;
                    case '\n': out += "\\n"; break;
                    case '\r': out += "\\r"; break;
                    case '\t': out += "\\t"; break;
                    default: out += c; break;
                }
            }
            return out;
        }
    };

} // namespace systems::leal::campello_widgets
