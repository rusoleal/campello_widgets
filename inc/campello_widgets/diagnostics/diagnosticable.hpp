#pragma once

#include <campello_widgets/diagnostics/diagnostics_node.hpp>

#include <memory>
#include <string>
#include <vector>

namespace systems::leal::campello_widgets
{

    /**
     * @brief Mixin that provides structured debug representation for an object.
     *
     * Equivalent to Flutter's Diagnosticable / DiagnosticableTree.
     *
     * Override debugFillProperties() to expose typed properties.
     * Override debugDescribeChildren() to expose child tree nodes.
     */
    class Diagnosticable
    {
    public:
        virtual ~Diagnosticable() = default;

        /**
         * @brief Add typed properties describing this object's state.
         *
         * Override in subclasses to expose configuration values.
         * Call the base implementation first, then add your own properties.
         */
        virtual void debugFillProperties(DiagnosticsPropertyBuilder& /*properties*/) const {}

        /**
         * @brief Return child DiagnosticsNode objects for tree visualization.
         *
         * Override if this object logically has child nodes in a diagnostic tree.
         */
        virtual std::vector<std::shared_ptr<DiagnosticsNode>> debugDescribeChildren() const
        {
            return {};
        }

        /**
         * @brief Build a full DiagnosticsNode for this object.
         *
         * Collects properties via debugFillProperties() and children via
         * debugDescribeChildren().
         */
        virtual std::shared_ptr<DiagnosticsNode> toDiagnosticsNode(const std::string& name = "") const
        {
            auto node = std::make_shared<DiagnosticsNode>();
            node->name = name.empty() ? toStringShort() : name;
            node->showName = showNameInDiagnostics();

            DiagnosticsPropertyBuilder builder;
            debugFillProperties(builder);
            node->properties = std::move(builder.properties);

            node->children = debugDescribeChildren();
            return node;
        }

        /**
         * @brief Short one-line description, typically the demangled type name.
         */
        virtual std::string toStringShort() const
        {
            return typeName();
        }

        /**
         * @brief Full multi-line tree representation of this node and descendants.
         */
        std::string toStringDeep() const
        {
            return toDiagnosticsNode()->toStringDeep();
        }

        /**
         * @brief Whether to show the name prefix in diagnostic output.
         */
        virtual bool showNameInDiagnostics() const { return true; }

    protected:
        /**
         * @brief Returns the demangled C++ type name.
         *
         * Uses compiler intrinsics when available. Falls back to typeid::name().
         */
        std::string typeName() const;
    };

} // namespace systems::leal::campello_widgets
