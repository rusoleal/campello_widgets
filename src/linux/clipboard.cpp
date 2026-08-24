#include <campello_widgets/ui/clipboard.hpp>

// Runtime dispatch to whichever backend is actually active -- this app
// decides X11 vs. Wayland at runtime (WAYLAND_DISPLAY env var, see
// run_app.cpp's runApp()), not at compile time, so Clipboard has to do the
// same rather than picking one unconditionally.
#include "linux_display_backend.hpp"

#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND
#include "wayland_clipboard.hpp"
#endif

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <sys/select.h>

#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace systems::leal::campello_widgets
{
    namespace
    {
        // ---------------------------------------------------------------
        // X11 CLIPBOARD selection owner.
        //
        // X11 clipboard "copy" isn't a system-wide buffer write the way
        // NSPasteboard/Win32 is -- it's ownership: XSetSelectionOwner()
        // just claims the CLIPBOARD selection, and the actual bytes only
        // ever move when some other client later asks for them via a
        // SelectionRequest event, which we have to be alive to answer for
        // as long as we're the owner. That means a persistent connection
        // and a background thread blocked in XNextEvent(), not a one-shot
        // call -- unlike getText() below, which is a self-contained,
        // transient connection since reading doesn't need to persist
        // anything.
        //
        // XInitThreads() makes Xlib safe to call from both this owner
        // thread (XNextEvent()) and whichever thread calls setText()
        // (XSetSelectionOwner()) on the same Display*.
        // ---------------------------------------------------------------
        struct X11ClipboardOwner
        {
            std::mutex  mutex;
            std::string text;

            Display* display = nullptr;
            Window   window  = 0;
            Atom     clipboard_atom = 0;
            Atom     utf8_atom      = 0;
            Atom     targets_atom   = 0;

            std::atomic<bool> start_attempted{false};
        };

        X11ClipboardOwner g_owner;

        void answerSelectionRequest(const XSelectionRequestEvent& req)
        {
            XSelectionEvent notify{};
            notify.type      = SelectionNotify;
            notify.display   = req.display;
            notify.requestor = req.requestor;
            notify.selection = req.selection;
            notify.target    = req.target;
            notify.time      = req.time;
            notify.property  = None; // default: refuse

            // Pre-ICCCM clients pass property == None and expect the
            // owner to pick a property name (conventionally the target
            // atom itself) rather than being told where to write.
            Atom prop = (req.property != None) ? req.property : req.target;

            if (req.target == g_owner.targets_atom)
            {
                Atom targets[3] = {g_owner.targets_atom, g_owner.utf8_atom, XA_STRING};
                XChangeProperty(g_owner.display, req.requestor, prop, XA_ATOM, 32,
                                 PropModeReplace, reinterpret_cast<unsigned char*>(targets), 3);
                notify.property = prop;
            }
            else if (req.target == g_owner.utf8_atom || req.target == XA_STRING)
            {
                std::string text_copy;
                {
                    std::lock_guard<std::mutex> lock(g_owner.mutex);
                    text_copy = g_owner.text;
                }
                // format=8 (byte-sized elements) with the target atom
                // itself as the property type, matching UTF8_STRING/
                // STRING's own convention -- same as every other X11
                // clipboard implementation's plain-text reply.
                XChangeProperty(g_owner.display, req.requestor, prop, req.target, 8,
                                 PropModeReplace,
                                 reinterpret_cast<const unsigned char*>(text_copy.data()),
                                 static_cast<int>(text_copy.size()));
                notify.property = prop;
            }
            // Any other target: notify.property stays None, telling the
            // requestor we can't provide it -- correct per ICCCM, not an
            // error.

            XSendEvent(g_owner.display, req.requestor, False, NoEventMask,
                       reinterpret_cast<XEvent*>(&notify));
            XFlush(g_owner.display);
        }

        void ownerThreadLoop()
        {
            for (;;)
            {
                XEvent ev;
                XNextEvent(g_owner.display, &ev); // blocks -- zero idle CPU
                if (ev.type == SelectionRequest)
                {
                    answerSelectionRequest(ev.xselectionrequest);
                }
                // SelectionClear (another client took ownership): nothing
                // to do -- we just stop being asked SelectionRequest for
                // this selection until setText() reclaims it.
            }
        }

        // Starts the owner connection + thread on first setText() call.
        // Returns false if X11 is unreachable (e.g. no DISPLAY).
        bool ensureOwnerStarted()
        {
            if (g_owner.start_attempted.exchange(true))
                return g_owner.display != nullptr;

            XInitThreads();
            g_owner.display = XOpenDisplay(nullptr);
            if (!g_owner.display) return false;

            g_owner.window = XCreateSimpleWindow(
                g_owner.display, DefaultRootWindow(g_owner.display), 0, 0, 1, 1, 0, 0, 0);
            g_owner.clipboard_atom = XInternAtom(g_owner.display, "CLIPBOARD", False);
            g_owner.utf8_atom      = XInternAtom(g_owner.display, "UTF8_STRING", False);
            g_owner.targets_atom   = XInternAtom(g_owner.display, "TARGETS", False);

            std::thread(ownerThreadLoop).detach(); // runs for the app's lifetime
            return true;
        }

        void setTextX11(const std::string& text)
        {
            if (!ensureOwnerStarted()) return;

            {
                std::lock_guard<std::mutex> lock(g_owner.mutex);
                g_owner.text = text;
            }
            // Reclaims ownership if a SelectionClear dropped it since the
            // last call; harmless no-op re-assertion if we're still owner.
            XSetSelectionOwner(g_owner.display, g_owner.clipboard_atom, g_owner.window, CurrentTime);
            XFlush(g_owner.display);
        }

        // Self-contained: its own transient connection + invisible window,
        // torn down before returning. Doesn't touch g_owner at all -- even
        // when we're the CLIPBOARD owner ourselves, our own ownerThreadLoop
        // answers this exactly like it would answer any other client.
        //
        // Bounded by a timeout since XNextEvent() has no native "wait up to
        // N ms" -- an unresponsive or nonexistent selection owner would
        // otherwise hang this call forever.
        std::string getTextX11()
        {
            Display* d = XOpenDisplay(nullptr);
            if (!d) return {};

            Window w = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0, 1, 1, 0, 0, 0);
            Atom clipboard_atom = XInternAtom(d, "CLIPBOARD", False);
            Atom utf8_atom      = XInternAtom(d, "UTF8_STRING", False);
            Atom prop_atom      = XInternAtom(d, "CAMPELLO_CLIPBOARD_XFER", False);

            XConvertSelection(d, clipboard_atom, utf8_atom, prop_atom, w, CurrentTime);
            XFlush(d);

            std::string result;
            const int fd = ConnectionNumber(d);
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);

            while (std::chrono::steady_clock::now() < deadline)
            {
                if (XPending(d))
                {
                    XEvent ev;
                    XNextEvent(d, &ev);
                    if (ev.type == SelectionNotify)
                    {
                        // property == None means the owner declined this
                        // target (or there is no owner) -- result stays
                        // empty, which is the correct "nothing to paste"
                        // outcome, not an error.
                        if (ev.xselection.property != None)
                        {
                            Atom actual_type;
                            int actual_format;
                            unsigned long nitems, bytes_after;
                            unsigned char* data = nullptr;
                            if (XGetWindowProperty(d, w, ev.xselection.property, 0, LONG_MAX, False,
                                                    AnyPropertyType, &actual_type, &actual_format,
                                                    &nitems, &bytes_after, &data) == Success && data)
                            {
                                result.assign(reinterpret_cast<char*>(data), nitems);
                                XFree(data);
                            }
                            XDeleteProperty(d, w, ev.xselection.property);
                        }
                        break;
                    }
                }
                else
                {
                    fd_set fds;
                    FD_ZERO(&fds);
                    FD_SET(fd, &fds);
                    timeval tv{0, 20000}; // 20ms poll granularity
                    select(fd + 1, &fds, nullptr, nullptr, &tv);
                }
            }

            XDestroyWindow(d, w);
            XCloseDisplay(d);
            return result;
        }

    } // namespace

    void Clipboard::setText(const std::string& text)
    {
#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND
        if (isRunningUnderWayland())
        {
            setWaylandClipboardText(text);
            return;
        }
#endif
        setTextX11(text);
    }

    std::string Clipboard::getText()
    {
#ifdef CAMPELLO_WIDGETS_HAS_WAYLAND
        if (isRunningUnderWayland())
            return getWaylandClipboardText();
#endif
        return getTextX11();
    }

} // namespace systems::leal::campello_widgets
