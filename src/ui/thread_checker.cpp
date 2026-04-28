#include <campello_widgets/ui/thread_checker.hpp>

#include <iostream>
#include <cstdlib>

namespace systems::leal::campello_widgets
{

ThreadChecker& ThreadChecker::instance() noexcept
{
    static ThreadChecker checker;
    return checker;
}

void ThreadChecker::bindToCurrentThread() noexcept
{
    bound_id_ = std::this_thread::get_id();
    bound_ = true;
}

bool ThreadChecker::isOnBoundThread() const noexcept
{
    return !bound_ || std::this_thread::get_id() == bound_id_;
}

void ThreadChecker::assertOnBoundThread(const char* context) const noexcept
{
    (void)context; // |context| is only used in the debug-build error message.
#ifndef NDEBUG
    if (!bound_)
        return;

    if (std::this_thread::get_id() != bound_id_)
    {
        std::cerr << "[campello_widgets] FATAL: " << context
                  << " called from a background thread (id="
                  << std::this_thread::get_id()
                  << "). All UI operations must happen on the bound thread (id="
                  << bound_id_ << ").\n";
        std::abort();
    }
#endif
}

} // namespace systems::leal::campello_widgets
