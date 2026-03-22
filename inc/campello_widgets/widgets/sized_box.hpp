#pragma once

#include <optional>
#include <campello_widgets/widgets/single_child_render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A box with a specified size.
     *
     * If `width` or `height` is omitted the corresponding axis sizes to the
     * maximum available from the parent constraints.
     */
    class SizedBox : public SingleChildRenderObjectWidget
    {
    public:
        std::optional<float> width;
        std::optional<float> height;

        SizedBox() = default;
        explicit SizedBox(std::optional<float> w,
                          std::optional<float> h = std::nullopt,
                          WidgetRef            c = nullptr)
        {
            width  = w;
            height = h;
            child  = std::move(c);
        }

        std::shared_ptr<RenderObject> createRenderObject() const override;
        void updateRenderObject(RenderObject& ro) const override;
    };

} // namespace systems::leal::campello_widgets
