#pragma once

#include <memory>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/size.hpp>
#include <campello_widgets/ui/box_fit.hpp>
#include <campello_widgets/ui/alignment.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_gpu { class Texture; }

namespace systems::leal::campello_widgets
{

    /**
     * @brief Displays a GPU texture.
     *
     * If `size` is zero (default) the image fills the available space from the
     * parent constraints. Otherwise it is drawn at exactly the given size.
     */
    class Image : public StatelessWidget
    {
    public:
        std::shared_ptr<campello_gpu::Texture> texture;
        Size      size      = Size::zero();
        BoxFit    fit       = BoxFit::fill;
        Alignment alignment = Alignment::center();
        float     opacity   = 1.0f;

        Image() = default;
        explicit Image(std::shared_ptr<campello_gpu::Texture> tex,
                       Size      s = Size::zero(),
                       BoxFit    f = BoxFit::fill,
                       Alignment a = Alignment::center(),
                       float     o = 1.0f)
            : texture(std::move(tex)), size(s), fit(f), alignment(a), opacity(o)
        {
            CW_ASSERT_MSG(o >= 0.0f && o <= 1.0f, "Image.opacity must be in [0.0, 1.0]");
        }
        WidgetRef build(BuildContext& context) const override;
        void debugValidate() const override
        {
            CW_ASSERT_MSG(opacity >= 0.0f && opacity <= 1.0f, "Image.opacity must be in [0.0, 1.0]");

        }

    };

} // namespace systems::leal::campello_widgets
