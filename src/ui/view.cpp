#include <campello_widgets/ui/view.hpp>
#include <campello_widgets/ui/pointer_dispatcher.hpp>
#include <campello_widgets/ui/focus_manager.hpp>
#include <campello_widgets/ui/text_input_manager.hpp>
#include <campello_widgets/ui/renderer.hpp>
#include <campello_widgets/widgets/element.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{
    namespace
    {
        // All views live on the UI thread (ThreadChecker), so plain statics
        // are enough for the registry. The ambient scope is thread_local so a
        // stray use from another thread (e.g. a worker finishing an image
        // decode) sees no scope rather than the UI thread's.
        std::vector<View*>&   registry() { static std::vector<View*> r; return r; }
        View*                 s_primary = nullptr;
        thread_local View*    t_scoped  = nullptr;
    }

    View::View()
        : pointer_dispatcher_(std::make_shared<PointerDispatcher>())
        , focus_manager_(std::make_shared<FocusManager>())
        , text_input_manager_(std::make_shared<TextInputManager>())
    {
        registry().push_back(this);
    }

    View::~View()
    {
        auto& r = registry();
        r.erase(std::remove(r.begin(), r.end(), this), r.end());

        if (renderer_) renderer_->setView(nullptr);

        // Drop the legacy fallbacks we installed (makePrimary()).
        if (s_primary == this)
        {
            PointerDispatcher::setActiveDispatcher(nullptr);
            FocusManager::setActiveManager(nullptr);
            TextInputManager::setActiveManager(nullptr);
            s_primary = nullptr;
        }
        if (t_scoped == this) t_scoped = nullptr;
    }

    void View::setRenderer(Renderer* renderer) noexcept
    {
        if (renderer_ == renderer) return;
        if (renderer_) renderer_->setView(nullptr);
        renderer_ = renderer;
        if (renderer_) renderer_->setView(this);
    }

    void View::setFrameRequestCallback(std::function<void()> callback)
    {
        frame_request_ = std::move(callback);
    }

    void View::requestFrame()
    {
        if (frame_request_) frame_request_();
    }

    void View::makePrimary() noexcept
    {
        s_primary = this;
        PointerDispatcher::setActiveDispatcher(pointer_dispatcher_.get());
        FocusManager::setActiveManager(focus_manager_.get());
        TextInputManager::setActiveManager(text_input_manager_.get());
    }

    View* View::current() noexcept
    {
        return t_scoped ? t_scoped : s_primary;
    }

    View* View::of(const BuildContext& context) noexcept
    {
        if (auto* element = dynamic_cast<const Element*>(&context))
            if (View* v = element->view(); v && isAlive(v)) return v;
        return current();
    }

    View* View::primary() noexcept
    {
        return s_primary;
    }

    View* View::scoped() noexcept
    {
        return t_scoped;
    }

    bool View::isAlive(const View* view) noexcept
    {
        if (!view) return false;
        const auto& r = registry();
        return std::find(r.begin(), r.end(), view) != r.end();
    }

    std::vector<View*> View::all()
    {
        return registry();
    }

    View::Scope::Scope(View* view) noexcept
        : previous_(t_scoped)
    {
        t_scoped = view;
    }

    View::Scope::~Scope()
    {
        t_scoped = previous_;
    }


    void View::addInputBlocker(View* blocker)
    {
        if (!blocker || blocker == this) return;
        input_blockers_.push_back(blocker);
        pointer_dispatcher_->setInputBlocked(true);
        focus_manager_->setInputBlocked(true);
    }

    void View::removeInputBlocker(View* blocker)
    {
        auto it = std::find(input_blockers_.rbegin(), input_blockers_.rend(), blocker);
        if (it != input_blockers_.rend()) input_blockers_.erase(std::next(it).base());
        if (inputBlocker() == nullptr)
        {
            input_blockers_.clear();
            pointer_dispatcher_->setInputBlocked(false);
            focus_manager_->setInputBlocked(false);
        }
    }

    View* View::inputBlocker() const noexcept
    {
        for (auto it = input_blockers_.rbegin(); it != input_blockers_.rend(); ++it)
            if (isAlive(*it)) return *it;
        return nullptr;
    }
} // namespace systems::leal::campello_widgets
