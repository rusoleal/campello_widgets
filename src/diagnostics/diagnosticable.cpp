#include <campello_widgets/diagnostics/diagnosticable.hpp>

#include <typeinfo>

#if defined(__GNUC__) || defined(__clang__)
#include <cxxabi.h>
#include <cstdlib>
#endif

namespace systems::leal::campello_widgets
{

    std::string Diagnosticable::typeName() const
    {
        const std::type_info& ti = typeid(*this);

#if defined(__GNUC__) || defined(__clang__)
        int status = 0;
        char* demangled = abi::__cxa_demangle(ti.name(), nullptr, nullptr, &status);
        if (status == 0 && demangled)
        {
            std::string result(demangled);
            std::free(demangled);
            return result;
        }
#endif
        return ti.name();
    }

} // namespace systems::leal::campello_widgets
