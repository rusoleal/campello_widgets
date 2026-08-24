#include <campello_widgets/ui/text_editing_controller.hpp>

#include <algorithm>
#include <chrono>

namespace systems::leal::campello_widgets
{
    namespace
    {
        uint64_t nowMs()
        {
            using namespace std::chrono;
            return static_cast<uint64_t>(
                duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
        }
    }

    TextEditingController* TextEditingController::s_focused_ = nullptr;
    bool TextEditingController::s_focused_obscured_ = false;

    TextEditingController::TextEditingController(std::string initial_text)
        : text_(std::move(initial_text))
        , selection_start_(static_cast<int>(text_.size()))
        , selection_end_(static_cast<int>(text_.size()))
    {}

    TextEditingController::~TextEditingController()
    {
        if (s_focused_ == this)
            s_focused_ = nullptr;
    }

    void TextEditingController::setText(std::string text)
    {
        if (text_ == text) return;
        text_            = std::move(text);
        selection_start_ = static_cast<int>(text_.size());
        selection_end_   = selection_start_;
        // A fresh value assigned from application code (e.g. loading a
        // different document into a reused controller) isn't a "prior
        // state" of what's now displayed -- undoing into it would jump
        // into unrelated content.
        undo_stack_.clear();
        redo_stack_.clear();
        notifyListeners();
    }

    void TextEditingController::clear()
    {
        setText("");
    }

    void TextEditingController::setSelection(int start, int end)
    {
        int sz = static_cast<int>(text_.size());
        start  = std::clamp(start, 0, sz);
        end    = std::clamp(end,   0, sz);
        if (selection_start_ == start && selection_end_ == end) return;
        selection_start_ = start;
        selection_end_   = end;
        notifyListeners();
    }

    void TextEditingController::selectAll()
    {
        setSelection(0, static_cast<int>(text_.size()));
    }

    std::string TextEditingController::selectedText() const
    {
        if (!hasSelection()) return {};
        int lo = std::min(selection_start_, selection_end_);
        int hi = std::max(selection_start_, selection_end_);
        return text_.substr(static_cast<size_t>(lo), static_cast<size_t>(hi - lo));
    }

    void TextEditingController::insertText(std::string_view to_insert)
    {
        if (to_insert.empty()) return;

        recordUndoSnapshot();

        int lo = std::min(selection_start_, selection_end_);
        int hi = std::max(selection_start_, selection_end_);

        text_.replace(static_cast<size_t>(lo),
                      static_cast<size_t>(hi - lo),
                      to_insert.data(),
                      to_insert.size());

        int cursor       = lo + static_cast<int>(to_insert.size());
        selection_start_ = cursor;
        selection_end_   = cursor;
        notifyListeners();
    }

    void TextEditingController::deleteBackward()
    {
        bool hasSel = hasSelection();
        if (!hasSel && selection_start_ <= 0)
            return; // nothing to delete, no notification needed

        recordUndoSnapshot();

        if (hasSel)
        {
            int lo = std::min(selection_start_, selection_end_);
            int hi = std::max(selection_start_, selection_end_);
            text_.erase(static_cast<size_t>(lo), static_cast<size_t>(hi - lo));
            selection_start_ = lo;
            selection_end_   = lo;
        }
        else
        {
            text_.erase(static_cast<size_t>(selection_start_ - 1), 1);
            --selection_start_;
            --selection_end_;
        }
        notifyListeners();
    }

    void TextEditingController::deleteForward()
    {
        bool hasSel = hasSelection();
        if (!hasSel && selection_start_ >= static_cast<int>(text_.size()))
            return; // nothing to delete

        recordUndoSnapshot();

        if (hasSel)
        {
            int lo = std::min(selection_start_, selection_end_);
            int hi = std::max(selection_start_, selection_end_);
            text_.erase(static_cast<size_t>(lo), static_cast<size_t>(hi - lo));
            selection_start_ = lo;
            selection_end_   = lo;
        }
        else
        {
            text_.erase(static_cast<size_t>(selection_start_), 1);
        }
        notifyListeners();
    }

    std::string TextEditingController::composingText() const
    {
        if (!isComposing()) return {};
        int lo = std::min(composing_start_, composing_end_);
        int hi = std::max(composing_start_, composing_end_);
        return text_.substr(static_cast<size_t>(lo), static_cast<size_t>(hi - lo));
    }

    void TextEditingController::beginComposing()
    {
        if (isComposing()) return; // Already composing

        // Snapshot before the composition starts, not per-keystroke inside
        // it -- an IME composition (e.g. building an accented or CJK
        // character) is one logical edit, undone as a whole back to
        // whatever text preceded it.
        recordUndoSnapshot();

        // Start composing at current cursor position
        int pos = selection_end_;
        composing_start_ = pos;
        composing_end_ = pos;
        // Note: don't notify here, composition is invisible state until text is added
    }

    void TextEditingController::updateComposingText(std::string_view text)
    {
        if (!isComposing())
        {
            // Auto-begin composing if not already started
            beginComposing();
        }

        // Replace the composing range with new text
        int lo = std::min(composing_start_, composing_end_);
        int hi = std::max(composing_start_, composing_end_);

        text_.replace(static_cast<size_t>(lo),
                      static_cast<size_t>(hi - lo),
                      text.data(),
                      text.size());

        // Update composing range to encompass new text
        composing_end_ = lo + static_cast<int>(text.size());
        composing_start_ = lo;

        // Place cursor at end of composing text
        selection_start_ = composing_end_;
        selection_end_ = composing_end_;

        notifyListeners();
    }

    void TextEditingController::setComposingRange(int start, int end, int selection_offset)
    {
        int sz = static_cast<int>(text_.size());
        composing_start_ = std::clamp(start, 0, sz);
        composing_end_ = std::clamp(end, 0, sz);

        // Set selection within composing range
        int sel = composing_start_ + selection_offset;
        sel = std::clamp(sel, composing_start_, composing_end_);
        selection_start_ = sel;
        selection_end_ = sel;

        notifyListeners();
    }

    void TextEditingController::commitComposing()
    {
        if (!isComposing()) return;

        // Composing text becomes regular text - just clear the composing markers
        composing_start_ = -1;
        composing_end_ = -1;

        // Keep selection where it is (at the end of the former composing text)
        notifyListeners();
    }

    void TextEditingController::cancelComposing()
    {
        if (!isComposing()) return;

        // Remove the composing text
        int lo = std::min(composing_start_, composing_end_);
        int hi = std::max(composing_start_, composing_end_);

        text_.erase(static_cast<size_t>(lo), static_cast<size_t>(hi - lo));

        // Restore selection to where composition started
        selection_start_ = lo;
        selection_end_ = lo;

        composing_start_ = -1;
        composing_end_ = -1;

        notifyListeners();
    }

    void TextEditingController::recordUndoSnapshot()
    {
        uint64_t now = nowMs();
        bool coalesce = !undo_stack_.empty()
                     && (now - last_snapshot_time_ms_) < kUndoCoalesceWindowMs;

        if (!coalesce)
        {
            undo_stack_.push_back({text_, selection_start_, selection_end_});
            if (undo_stack_.size() > kMaxUndoHistory)
                undo_stack_.erase(undo_stack_.begin());
        }

        // Any new edit invalidates the redo future, coalesced or not --
        // matches standard editor behavior.
        redo_stack_.clear();
        last_snapshot_time_ms_ = now;
    }

    void TextEditingController::undo()
    {
        if (undo_stack_.empty()) return;

        redo_stack_.push_back({text_, selection_start_, selection_end_});

        EditSnapshot snap = std::move(undo_stack_.back());
        undo_stack_.pop_back();

        text_            = std::move(snap.text);
        selection_start_ = snap.selection_start;
        selection_end_   = snap.selection_end;

        // Without this, a keystroke landing within the coalescing window
        // right after an undo would silently merge into the edit that was
        // just undone instead of starting its own step.
        last_snapshot_time_ms_ = 0;

        notifyListeners();
    }

    void TextEditingController::redo()
    {
        if (redo_stack_.empty()) return;

        undo_stack_.push_back({text_, selection_start_, selection_end_});

        EditSnapshot snap = std::move(redo_stack_.back());
        redo_stack_.pop_back();

        text_            = std::move(snap.text);
        selection_start_ = snap.selection_start;
        selection_end_   = snap.selection_end;

        last_snapshot_time_ms_ = 0;

        notifyListeners();
    }

    uint64_t TextEditingController::addListener(std::function<void()> fn)
    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        uint64_t id = next_listener_id_++;
        listeners_.emplace_back(id, std::move(fn));
        return id;
    }

    void TextEditingController::removeListener(uint64_t id)
    {
        std::lock_guard<std::mutex> lock(listeners_mutex_);
        listeners_.erase(
            std::remove_if(listeners_.begin(), listeners_.end(),
                           [id](const auto& p) { return p.first == id; }),
            listeners_.end());
    }

    void TextEditingController::notifyListeners()
    {
        std::vector<std::pair<uint64_t, std::function<void()>>> copy;
        {
            std::lock_guard<std::mutex> lock(listeners_mutex_);
            copy = listeners_; // snapshot in case a listener mutates the list
        }
        for (auto& [lid, fn] : copy)
            fn();
    }

} // namespace systems::leal::campello_widgets
