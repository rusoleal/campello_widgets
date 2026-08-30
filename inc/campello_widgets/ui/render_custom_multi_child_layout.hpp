#pragma once

#include <vector>
#include <string>
#include <memory>
#include <campello_widgets/ui/render_box.hpp>
#include <campello_widgets/ui/multi_child_layout_delegate.hpp>

namespace systems::leal::campello_widgets
{

    /**
     * @brief A RenderBox that lays out an id-tagged set of children via a
     * `MultiChildLayoutDelegate`, rather than a fixed layout algorithm.
     *
     * Child storage/paint/hit-test mirrors `RenderStack`'s; layout is
     * delegated entirely to `delegate->performLayout()`, which calls back
     * into this object (as `MultiChildLayoutContext`) per child by id.
     *
     * Matches Flutter's `CustomMultiChildLayout` widget's render object.
     */
    class RenderCustomMultiChildLayout : public RenderBox, public MultiChildLayoutContext
    {
    public:
        std::shared_ptr<MultiChildLayoutDelegate> delegate;

        void insertChild(const std::string& id, std::shared_ptr<RenderBox> box);
        void clearChildren();

        void performLayout() override;
        void performPaint(PaintContext& context, const Offset& offset) override;
        bool hitTestChildren(HitTestResult& result, const Offset& position) override;
        void visitRenderChildren(const std::function<void(RenderBox*)>& visitor) const override;

        // MultiChildLayoutContext
        bool hasChild(const std::string& id) const override;
        Size layoutChild(const std::string& id, const BoxConstraints& constraints) override;
        void positionChild(const std::string& id, const Offset& offset) override;

    private:
        struct Entry
        {
            std::string                id;
            std::shared_ptr<RenderBox> box;
            Offset                     offset;
        };

        Entry* findEntry(const std::string& id);

        std::vector<Entry> children_;
    };

} // namespace systems::leal::campello_widgets
