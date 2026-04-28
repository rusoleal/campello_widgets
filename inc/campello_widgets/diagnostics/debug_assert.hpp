#pragma once

#include <campello_widgets/diagnostics/widget_inspector.hpp>

#include <iostream>

#if defined(_WIN32)
    #include <intrin.h>
    #define CW_DEBUG_BREAK() __debugbreak()
#else
    #include <csignal>
    #define CW_DEBUG_BREAK() std::raise(SIGTRAP)
#endif

namespace systems::leal::campello_widgets
{

    /**
     * @brief Assertion macros that dump the widget tree on failure.
     *
     * These are debug-build only. In release builds they compile away to nothing.
     *
     * Usage:
     * @code
     *   CW_ASSERT(child_ != nullptr);
     *   CW_ASSERT_MSG(size_.width >= 0, "width must be non-negative");
     * @endcode
     */

#ifdef NDEBUG

    #define CW_ASSERT(condition) ((void)0)
    #define CW_ASSERT_MSG(condition, message) ((void)0)

#else

    #define CW_ASSERT(condition) \
        do { \
            if (!(condition)) { \
                std::cerr << "\n========== ASSERTION FAILED ==========\n"; \
                std::cerr << "File: " << __FILE__ << ":" << __LINE__ << "\n"; \
                std::cerr << "Condition: " #condition "\n"; \
                std::cerr << "\n"; \
                WidgetInspector::instance().dumpWidgetTree(std::cerr); \
                std::cerr << "========================================\n"; \
                CW_DEBUG_BREAK(); \
            } \
        } while (0)

    #define CW_ASSERT_MSG(condition, message) \
        do { \
            if (!(condition)) { \
                std::cerr << "\n========== ASSERTION FAILED ==========\n"; \
                std::cerr << "File: " << __FILE__ << ":" << __LINE__ << "\n"; \
                std::cerr << "Condition: " #condition "\n"; \
                std::cerr << "Message: " << message << "\n"; \
                std::cerr << "\n"; \
                WidgetInspector::instance().dumpWidgetTree(std::cerr); \
                std::cerr << "========================================\n"; \
                CW_DEBUG_BREAK(); \
            } \
        } while (0)

#endif

} // namespace systems::leal::campello_widgets
