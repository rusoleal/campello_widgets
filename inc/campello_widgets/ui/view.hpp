#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace systems::leal::campello_widgets
{
    class PointerDispatcher;
    class FocusManager;
    class TextInputManager;
    class Renderer;
    class RenderObject;
    class BuildContext;

    /**
     * @brief One window's worth of per-window UI state (Phase 17, multi-window).
     *
     * Each native window (or, on the web, each canvas) gets one `View`. It
     * owns the subsystems that used to be process-wide singletons but are
     * really per-window: pointer dispatch (hit testing, captures, gesture
     * arena), focus, text input (the IME belongs to one window), the
     * window's `Renderer`, and its "please draw a frame" hook.
     *
     * All views share one UI thread and one element tree (the architecture
     * decision recorded in TODO.md, Phase 17, modelled on Flutter's
     * multi-view work). Shared, process-wide state stays global:
     * `TickerScheduler`, `DragManager`, `ImageCache`, `HardwareKeyboard`,
     * `ThreadChecker`.
     *
     * **How code finds its view.** Every `RenderObject` and `Element` records
     * an owner view (`RenderObject::view()`, `View::of(context)`). It is
     * inherited from the parent when the object joins the tree, so the parent
     * is always the source of truth. Only roots (no parent) take the
     * *ambient* view: the one a `View::Scope` made current, e.g. while the
     * platform runner mounts that window's tree.
     *
     * **Single-window apps and tests.** The old static accessors
     * (`PointerDispatcher::activeDispatcher()`, `FocusManager::activeManager()`,
     * ...) keep working as fallbacks: `makePrimary()` installs a view's
     * subsystems there, and objects with no view (unit tests that build a
     * render tree by hand) use whatever the test installed, as before.
     */
    class View
    {
    public:
        View();
        ~View();

        View(const View&)            = delete;
        View& operator=(const View&) = delete;

        // ------------------------------------------------------------------
        // Per-window subsystems
        // ------------------------------------------------------------------

        PointerDispatcher& pointerDispatcher() noexcept { return *pointer_dispatcher_; }
        FocusManager&      focusManager() noexcept      { return *focus_manager_; }
        TextInputManager&  textInputManager() noexcept  { return *text_input_manager_; }

        /// Shared ownership, for platform glue that keeps its own references
        /// (e.g. the macOS MTKView subclass that forwards events).
        const std::shared_ptr<PointerDispatcher>& sharedPointerDispatcher() const noexcept { return pointer_dispatcher_; }
        const std::shared_ptr<FocusManager>&      sharedFocusManager() const noexcept      { return focus_manager_; }
        const std::shared_ptr<TextInputManager>&  sharedTextInputManager() const noexcept  { return text_input_manager_; }

        /// The window's renderer. Not owned: the platform runner creates it
        /// after mounting the tree (it needs the root RenderBox) and must
        /// clear it (setRenderer(nullptr)) before destroying it.
        void      setRenderer(Renderer* renderer) noexcept;
        Renderer* renderer() const noexcept { return renderer_; }

        // ------------------------------------------------------------------
        // Frames
        // ------------------------------------------------------------------

        /// The platform's "draw one frame for this window" primitive
        /// (setNeedsDisplay:, InvalidateRect, requestAnimationFrame, ...).
        void setFrameRequestCallback(std::function<void()> callback);
        bool hasFrameRequestCallback() const noexcept { return static_cast<bool>(frame_request_); }

        /// Asks for a frame for this window only. A no-op if no callback is set.
        void requestFrame();

        // ------------------------------------------------------------------
        // Modal blocking (Phase 17)
        // ------------------------------------------------------------------

        /// A modal window (`blocker`'s) opened over this one: this view's
        /// pointer and key input is blocked until every blocker is removed.
        /// Platforms use inputBlocker() to bring the modal window forward
        /// when the user clicks this one.
        void addInputBlocker(View* blocker);
        void removeInputBlocker(View* blocker);
        /// The most recently added live blocker, or nullptr.
        View* inputBlocker() const noexcept;
        bool  isInputBlocked() const noexcept { return inputBlocker() != nullptr; }

        // ------------------------------------------------------------------
        // Single-window compatibility
        // ------------------------------------------------------------------

        /// Installs this view's subsystems as the process-wide fallbacks
        /// returned by the legacy static accessors. Platform runners call it
        /// for their (first) window. Cleared automatically on destruction.
        void makePrimary() noexcept;

        // ------------------------------------------------------------------
        // Lookup
        // ------------------------------------------------------------------

        /// The ambient view: the one a `View::Scope` made current on this
        /// thread, or the primary view if no scope is active, or nullptr.
        static View* current() noexcept;

        /// The view `context` belongs to (its element's owner view), falling
        /// back to `current()`.
        static View* of(const BuildContext& context) noexcept;

        /// The primary view (see makePrimary()), or nullptr.
        static View* primary() noexcept;

        /// Only the view made current by an active `View::Scope` on this
        /// thread, without the primary-view fallback; nullptr outside any
        /// scope. The legacy static accessors use this to prefer the window
        /// being processed over the process-wide fallback.
        static View* scoped() noexcept;

        /// Whether `view` points at a live View. Owner pointers are raw (a
        /// render object may outlive its window during teardown), so code
        /// that dereferences one in a destructor checks this first.
        static bool isAlive(const View* view) noexcept;

        /// Every live view, in creation order.
        static std::vector<View*> all();

        /// RAII: makes `view` the ambient view on this thread for the scope's
        /// lifetime (nests; restores the previous one).
        class Scope
        {
        public:
            explicit Scope(View* view) noexcept;
            ~Scope();
            Scope(const Scope&)            = delete;
            Scope& operator=(const Scope&) = delete;
        private:
            View* previous_;
        };

    private:
        std::shared_ptr<PointerDispatcher> pointer_dispatcher_;
        std::shared_ptr<FocusManager>      focus_manager_;
        std::shared_ptr<TextInputManager>  text_input_manager_;
        Renderer*                          renderer_ = nullptr;
        std::function<void()>              frame_request_;
        std::vector<View*>                 input_blockers_;
    };

} // namespace systems::leal::campello_widgets
