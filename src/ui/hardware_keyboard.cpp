#include <campello_widgets/ui/hardware_keyboard.hpp>

namespace systems::leal::campello_widgets
{

    HardwareKeyboard& HardwareKeyboard::current() noexcept
    {
        static HardwareKeyboard instance;
        return instance;
    }

    void HardwareKeyboard::updateModifiers(uint32_t modifiers) noexcept
    {
        modifiers_.store(modifiers, std::memory_order_relaxed);
    }

    uint32_t HardwareKeyboard::modifiers() const noexcept
    {
        return modifiers_.load(std::memory_order_relaxed);
    }

    bool HardwareKeyboard::isShiftPressed() const noexcept
    {
        return (modifiers() & KeyModifiers::shift) != 0;
    }

    bool HardwareKeyboard::isControlPressed() const noexcept
    {
        return (modifiers() & KeyModifiers::ctrl) != 0;
    }

    bool HardwareKeyboard::isAltPressed() const noexcept
    {
        return (modifiers() & KeyModifiers::alt) != 0;
    }

    bool HardwareKeyboard::isMetaPressed() const noexcept
    {
        return (modifiers() & KeyModifiers::meta) != 0;
    }

} // namespace systems::leal::campello_widgets
