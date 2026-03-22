#pragma once

#include <memory>
#include <campello_widgets/widgets/element.hpp>
#include <campello_widgets/widgets/render_object_widget.hpp>

namespace systems::leal::campello_widgets
{

    class RenderObject;

    /**
     * @brief Element counterpart for RenderObjectWidget.
     *
     * Owns the RenderObject for the lifetime of the element. Forwards lifecycle
     * events to the render object and calls `updateRenderObject()` on the widget
     * whenever the widget configuration changes.
     *
     * RenderObjectElement has no widget children — it is a leaf in the Element
     * tree. Its visual output comes entirely from the RenderObject it owns.
     */
    class RenderObjectElement : public Element
    {
    public:
        explicit RenderObjectElement(std::shared_ptr<const RenderObjectWidget> widget);

        void mount(Element* parent) override;
        void unmount() override;
        void update(WidgetRef new_widget) override;

        /** @brief The RenderObject owned by this element (raw pointer). */
        RenderObject* renderObject() const noexcept { return render_object_.get(); }

        /** @brief The RenderObject owned by this element (shared ownership). */
        std::shared_ptr<RenderObject> sharedRenderObject() const noexcept
        {
            return render_object_;
        }

        RenderObjectElement* nearestRenderObjectElement() noexcept override
        {
            return this;
        }

    protected:
        /**
         * @brief Default: leaf element with no widget children.
         *
         * SingleChildRenderObjectElement and MultiChildRenderObjectElement
         * override this to build their child subtrees.
         */
        void performBuild() override {}

        std::shared_ptr<RenderObject> render_object_;
    };

} // namespace systems::leal::campello_widgets
