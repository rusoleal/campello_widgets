#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <sstream>
#include <iomanip>
#include <vector>

#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/offset.hpp>
#include <campello_widgets/ui/box_constraints.hpp>
#include <campello_widgets/ui/edge_insets.hpp>

namespace systems::leal::campello_widgets
{

    // ------------------------------------------------------------------
    // DiagnosticLevel
    // ------------------------------------------------------------------

    enum class DiagnosticLevel
    {
        hidden,   // Never shown unless explicitly requested
        fine,     // Very detailed, usually hidden
        debug,    // Diagnostic helpers
        info,     // Normal properties
        warning,  // Suspicious values
        error,    // Invalid values
        summary,  // Top-level summary nodes
    };

    // ------------------------------------------------------------------
    // Base property
    // ------------------------------------------------------------------

    class DiagnosticPropertyBase
    {
    public:
        virtual ~DiagnosticPropertyBase() = default;

        std::string name;
        DiagnosticLevel level = DiagnosticLevel::info;
        bool showName = true;
        bool showSeparator = true;
        std::string tooltip;
        std::string ifNull;       // Text when value is "null" / empty
        std::string ifEmpty;      // Text when value is empty
        bool omitted = false;     // Set to true to hide this property

        virtual std::string valueToString() const = 0;

        std::string toString() const
        {
            if (omitted) return {};
            if (!showName) return valueToString();
            if (!showSeparator) return name + " " + valueToString();
            return name + ": " + valueToString();
        }

        bool isInteresting() const { return !omitted; }
    };

    using DiagnosticPropertyList = std::vector<std::unique_ptr<DiagnosticPropertyBase>>;

    // ------------------------------------------------------------------
    // Generic typed property
    // ------------------------------------------------------------------

    template<typename T>
    class DiagnosticProperty : public DiagnosticPropertyBase
    {
    public:
        T value;
        std::optional<T> defaultValue;

        DiagnosticProperty() = default;
        DiagnosticProperty(std::string name_, T value_)
            : value(std::move(value_))
        {
            name = std::move(name_);
        }
        DiagnosticProperty(std::string name_, T value_, T default_)
            : value(std::move(value_)), defaultValue(std::move(default_))
        {
            name = std::move(name_);
        }

        std::string valueToString() const override
        {
            return genericToString(value);
        }
    private:
        static std::string genericToString(const T& v)
        {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        }
    };

    // ------------------------------------------------------------------
    // Specialisations for framework types
    // ------------------------------------------------------------------

    template<>
    inline std::string DiagnosticProperty<bool>::valueToString() const
    {
        return value ? "true" : "false";
    }

    template<>
    inline std::string DiagnosticProperty<int>::valueToString() const
    {
        return std::to_string(value);
    }

    template<>
    inline std::string DiagnosticProperty<float>::valueToString() const
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << value;
        std::string s = oss.str();
        // Trim trailing zeros and possible trailing dot
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.push_back('0');
        return s;
    }

    template<>
    inline std::string DiagnosticProperty<double>::valueToString() const
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << value;
        std::string s = oss.str();
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.push_back('0');
        return s;
    }

    template<>
    inline std::string DiagnosticProperty<std::string>::valueToString() const
    {
        if (value.empty() && !ifEmpty.empty()) return ifEmpty;
        return value;
    }

    template<>
    inline std::string DiagnosticProperty<Color>::valueToString() const
    {
        std::ostringstream oss;
        oss << "Color(0x" << std::hex << std::setfill('0') << std::setw(8)
            << (static_cast<uint32_t>(value.a * 255.0f) << 24 |
                static_cast<uint32_t>(value.r * 255.0f) << 16 |
                static_cast<uint32_t>(value.g * 255.0f) << 8  |
                static_cast<uint32_t>(value.b * 255.0f))
            << ")";
        return oss.str();
    }

    template<>
    inline std::string DiagnosticProperty<Size>::valueToString() const
    {
        std::ostringstream oss;
        oss << "Size(" << DiagnosticProperty<float>("", value.width).valueToString()
            << ", " << DiagnosticProperty<float>("", value.height).valueToString() << ")";
        return oss.str();
    }

    template<>
    inline std::string DiagnosticProperty<Offset>::valueToString() const
    {
        std::ostringstream oss;
        oss << "Offset(" << DiagnosticProperty<float>("", value.x).valueToString()
            << ", " << DiagnosticProperty<float>("", value.y).valueToString() << ")";
        return oss.str();
    }

    template<>
    inline std::string DiagnosticProperty<BoxConstraints>::valueToString() const
    {
        std::ostringstream oss;
        oss << "BoxConstraints(";
        if (value.min_width == value.max_width)
            oss << "w=" << DiagnosticProperty<float>("", value.min_width).valueToString();
        else
            oss << DiagnosticProperty<float>("", value.min_width).valueToString()
                << "<=w<=" << DiagnosticProperty<float>("", value.max_width).valueToString();
        oss << ", ";
        if (value.min_height == value.max_height)
            oss << "h=" << DiagnosticProperty<float>("", value.min_height).valueToString();
        else
            oss << DiagnosticProperty<float>("", value.min_height).valueToString()
                << "<=h<=" << DiagnosticProperty<float>("", value.max_height).valueToString();
        oss << ")";
        return oss.str();
    }

    template<>
    inline std::string DiagnosticProperty<EdgeInsets>::valueToString() const
    {
        std::ostringstream oss;
        oss << "EdgeInsets(" << DiagnosticProperty<float>("", value.left).valueToString()
            << ", " << DiagnosticProperty<float>("", value.top).valueToString()
            << ", " << DiagnosticProperty<float>("", value.right).valueToString()
            << ", " << DiagnosticProperty<float>("", value.bottom).valueToString() << ")";
        return oss.str();
    }

    // ------------------------------------------------------------------
    // Convenience aliases
    // ------------------------------------------------------------------

    using StringProperty = DiagnosticProperty<std::string>;
    using DoubleProperty = DiagnosticProperty<double>;
    using IntProperty    = DiagnosticProperty<int>;
    using BoolProperty   = DiagnosticProperty<bool>;
    using ColorProperty  = DiagnosticProperty<Color>;
    using SizeProperty   = DiagnosticProperty<Size>;
    using OffsetProperty = DiagnosticProperty<Offset>;
    using ConstraintsProperty = DiagnosticProperty<BoxConstraints>;

    // Convenience: nullable property (shows ifNull text when empty optional)
    template<typename T>
    class NullableDiagnosticProperty : public DiagnosticPropertyBase
    {
    public:
        std::optional<T> value;
        std::optional<T> defaultValue;

        NullableDiagnosticProperty() = default;
        NullableDiagnosticProperty(std::string name_, std::optional<T> value_)
            : value(std::move(value_))
        {
            name = std::move(name_);
        }

        std::string valueToString() const override
        {
            if (!value.has_value())
                return ifNull.empty() ? "null" : ifNull;
            return DiagnosticProperty<T>("", *value).valueToString();
        }
    };

    // ------------------------------------------------------------------
    // FlagProperty — boolean shown as a presence/absence flag
    // ------------------------------------------------------------------

    class FlagProperty : public DiagnosticPropertyBase
    {
    public:
        bool value = false;
        std::string ifTrue;
        std::string ifFalse;

        FlagProperty() = default;
        FlagProperty(std::string name_, bool value_, std::string ifTrue_, std::string ifFalse_ = {})
            : value(value_), ifTrue(std::move(ifTrue_)), ifFalse(std::move(ifFalse_))
        {
            name = std::move(name_);
        }

        std::string valueToString() const override
        {
            if (value) return ifTrue.empty() ? name : ifTrue;
            return ifFalse.empty() ? "not " + name : ifFalse;
        }
    };

    // ------------------------------------------------------------------
    // MessageProperty — free-form text
    // ------------------------------------------------------------------

    class MessageProperty : public DiagnosticPropertyBase
    {
    public:
        std::string message;

        MessageProperty() = default;
        explicit MessageProperty(std::string message_)
            : message(std::move(message_))
        {
            showName = false;
            showSeparator = false;
        }
        MessageProperty(std::string name_, std::string message_)
            : message(std::move(message_))
        {
            name = std::move(name_);
        }

        std::string valueToString() const override { return message; }
    };

    // ------------------------------------------------------------------
    // EnumProperty — enum values with string conversion
    // ------------------------------------------------------------------

    template<typename T>
    class EnumProperty : public DiagnosticPropertyBase
    {
    public:
        T value;
        std::optional<T> defaultValue;
        std::function<std::string(T)> enumToString;

        EnumProperty() = default;
        EnumProperty(std::string name_, T value_, std::function<std::string(T)> toString_)
            : value(value_), enumToString(std::move(toString_))
        {
            name = std::move(name_);
        }

        std::string valueToString() const override
        {
            if (enumToString) return enumToString(value);
            std::ostringstream oss;
            oss << static_cast<std::underlying_type_t<T>>(value);
            return oss.str();
        }
    };

    // ------------------------------------------------------------------
    // DiagnosticsPropertyBuilder — helper to collect properties
    // ------------------------------------------------------------------

    class DiagnosticsPropertyBuilder
    {
    public:
        DiagnosticPropertyList properties;

        template<typename T>
        void add(std::unique_ptr<T> prop)
        {
            static_assert(std::is_base_of_v<DiagnosticPropertyBase, T>);
            properties.push_back(std::move(prop));
        }
    };

} // namespace systems::leal::campello_widgets
