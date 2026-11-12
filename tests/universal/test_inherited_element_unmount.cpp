// Regression: unmounting an InheritedElement must unmount its subtree.
// It used to stop at the InheritedElement, so States under a MediaQuery /
// Theme were never disposed when that subtree was removed (found via the
// Phase 17 Window widget).

#include <gtest/gtest.h>
#include <campello_widgets/widgets/media_query.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/element.hpp>

namespace cw = systems::leal::campello_widgets;

namespace
{
    class InhUnmountProbe : public cw::StatefulWidget
    {
    public:
        int* disposed = nullptr;
        std::unique_ptr<cw::StateBase> createState() const override;
    };
    class InhUnmountProbeState : public cw::State<InhUnmountProbe>
    {
    public:
        void dispose() override { ++*widget().disposed; }
        cw::WidgetRef build(cw::BuildContext&) override
        {
            return std::make_shared<cw::SizedBox>(1.0f, 1.0f);
        }
    };
    std::unique_ptr<cw::StateBase> InhUnmountProbe::createState() const
    {
        return std::make_unique<InhUnmountProbeState>();
    }
}

TEST(InheritedElement, UnmountDisposesStatesBelowIt)
{
    int disposed = 0;
    auto probe = std::make_shared<InhUnmountProbe>();
    probe->disposed = &disposed;
    auto root = std::make_shared<cw::MediaQuery>(cw::MediaQueryData{}, probe)->createElement();
    root->mount(nullptr);
    ASSERT_NE(root->firstChildElement(), nullptr);

    root->unmount();
    EXPECT_EQ(disposed, 1);
    EXPECT_EQ(root->firstChildElement(), nullptr);
}
