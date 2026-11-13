// Regression: a State's dispose() that touches an ancestor being torn down
// in the same unmount (e.g. PopupWindowAnchor removing its OverlayEntry from
// the Overlay that is itself being unmounted) must not rebuild that ancestor.
//
// In tests there is no frame callback, so markNeedsBuild() rebuilds
// synchronously: the ancestor used to rebuild in the middle of its own
// unmount and rewrite the child list MultiChildRenderObjectElement::unmount()
// was iterating (use after free; it crashed under the Windows debug heap in
// PopupWindowAnchor.FallsBackToTheWindowsOverlayWithoutNativePopups and
// passed silently on macOS).

#include <gtest/gtest.h>
#include <campello_widgets/widgets/column.hpp>
#include <campello_widgets/widgets/sized_box.hpp>
#include <campello_widgets/widgets/stateful_widget.hpp>
#include <campello_widgets/widgets/element.hpp>

#include <functional>

namespace cw = systems::leal::campello_widgets;

namespace
{
    // A leaf whose dispose() runs a callback.
    class DisposeHook : public cw::StatefulWidget
    {
    public:
        std::function<void()> on_dispose;
        std::unique_ptr<cw::StateBase> createState() const override;
    };
    class DisposeHookState : public cw::State<DisposeHook>
    {
    public:
        void dispose() override { if (widget().on_dispose) widget().on_dispose(); }
        cw::WidgetRef build(cw::BuildContext&) override { return std::make_shared<cw::SizedBox>(1.0f, 1.0f); }
    };
    std::unique_ptr<cw::StateBase> DisposeHook::createState() const { return std::make_unique<DisposeHookState>(); }

    // The ancestor: builds a Column of hooks; each hook's dispose() calls
    // setState() on it, as removing an OverlayEntry does on its Overlay.
    class Host : public cw::StatefulWidget
    {
    public:
        int* builds = nullptr;
        std::function<void()>* poke = nullptr;  ///< set to "setState on the host"
        std::unique_ptr<cw::StateBase> createState() const override;
    };
    class HostState : public cw::State<Host>
    {
    public:
        void initState() override { *widget().poke = [this] { setState([] {}); }; }

        cw::WidgetRef build(cw::BuildContext&) override
        {
            ++*widget().builds;
            std::vector<cw::WidgetRef> children;
            for (int i = 0; i < 3; ++i)
            {
                auto hook = std::make_shared<DisposeHook>();
                hook->on_dispose = [this] { setState([] {}); };
                children.push_back(hook);
            }
            return std::make_shared<cw::Column>(children);
        }
    };
    std::unique_ptr<cw::StateBase> Host::createState() const { return std::make_unique<HostState>(); }
}

TEST(ElementUnmount, DisposeTouchingAnAncestorBeingUnmountedDoesNotRebuildIt)
{
    int builds = 0;
    auto host  = std::make_shared<Host>();
    std::function<void()> poke;
    host->builds = &builds;
    host->poke   = &poke;
    auto root = host->createElement();
    root->mount(nullptr);
    // One ordinary rebuild first (synchronous here: no frame callback), like
    // an Overlay that has had entries inserted: the element is clean again,
    // so the next markNeedsBuild() would really rebuild it.
    poke();
    ASSERT_EQ(builds, 2);

    root->unmount();
    EXPECT_EQ(builds, 2) << "the host rebuilt while it was being unmounted";
}
