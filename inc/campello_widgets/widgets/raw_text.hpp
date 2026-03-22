#pragma once

#include <memory>
#include <string>
#include <campello_widgets/widgets/render_object_widget.hpp>
#include <campello_widgets/ui/text_span.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A widget that renders a styled text string.
     *
     * RawText maps directly to `RenderText` and issues a `DrawTextCmd`
     * each paint pass. For most use cases prefer the higher-level `Text`
     * widget (Phase 6), which adds text alignment and overflow handling.
     *
     * **Usage:**
     * @code
     * auto label = RawText::create("Hello!", TextStyle{Color::white(), 16.0f});
     * @endcode
     */
    class RawText : public RenderObjectWidget
    {
    public:
        TextSpan span;

        static std::shared_ptr<RawText> create(std::string text,
                                               TextStyle   style = {})
        {
            auto w   = std::make_shared<RawText>();
            w->span  = TextSpan{std::move(text), std::move(style)};
            return w;
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& render_object) const override;
    };

} // namespace systems::leal::campello_widgets
