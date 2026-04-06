#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/text_editing_controller.hpp>

namespace systems::leal::campello_widgets
{

    TextInputManager* TextInputManager::s_active_manager_ = nullptr;

    void TextInputManager::setActiveManager(TextInputManager* manager) noexcept
    {
        s_active_manager_ = manager;
    }

    TextInputManager* TextInputManager::activeManager() noexcept
    {
        return s_active_manager_;
    }

    void TextInputManager::registerInputTarget(const InputTarget& target)
    {
        current_target_ = target;
    }

    void TextInputManager::unregisterInputTarget()
    {
        current_target_ = InputTarget{};
    }

    bool TextInputManager::beginComposing()
    {
        if (!current_target_.controller) return false;
        current_target_.controller->beginComposing();
        return true;
    }

    bool TextInputManager::updateComposingText(std::string_view text)
    {
        if (!current_target_.controller) return false;
        current_target_.controller->updateComposingText(text);
        return true;
    }

    bool TextInputManager::commitComposing()
    {
        if (!current_target_.controller) return false;
        current_target_.controller->commitComposing();
        return true;
    }

    bool TextInputManager::cancelComposing()
    {
        if (!current_target_.controller) return false;
        current_target_.controller->cancelComposing();
        return true;
    }

    bool TextInputManager::insertText(std::string_view text)
    {
        if (!current_target_.controller) return false;
        current_target_.controller->insertText(text);
        return true;
    }

    bool TextInputManager::isComposing() const
    {
        if (!current_target_.controller) return false;
        return current_target_.controller->isComposing();
    }

    std::array<float, 4> TextInputManager::getCharacterRect(int byte_offset) const
    {
        if (current_target_.get_character_rect)
        {
            return current_target_.get_character_rect(byte_offset);
        }
        return {0.0f, 0.0f, 0.0f, 0.0f};
    }

} // namespace systems::leal::campello_widgets
