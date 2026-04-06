#include <campello_widgets/ui/text_editing_controller.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{

    TextEditingController::TextEditingController(std::string initial_text)
        : text_(std::move(initial_text))
        , selection_start_(static_cast<int>(text_.size()))
        , selection_end_(static_cast<int>(text_.size()))
    {}

    void TextEditingController::setText(std::string text)
    {
        if (text_ == text) return;
        text_            = std::move(text);
        selection_start_ = static_cast<int>(text_.size());
        selection_end_   = selection_start_;
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
        if (hasSelection())
        {
            int lo = std::min(selection_start_, selection_end_);
            int hi = std::max(selection_start_, selection_end_);
            text_.erase(static_cast<size_t>(lo), static_cast<size_t>(hi - lo));
            selection_start_ = lo;
            selection_end_   = lo;
        }
        else if (selection_start_ > 0)
        {
            text_.erase(static_cast<size_t>(selection_start_ - 1), 1);
            --selection_start_;
            --selection_end_;
        }
        else
        {
            return; // nothing to delete, no notification needed
        }
        notifyListeners();
    }

    void TextEditingController::deleteForward()
    {
        if (hasSelection())
        {
            int lo = std::min(selection_start_, selection_end_);
            int hi = std::max(selection_start_, selection_end_);
            text_.erase(static_cast<size_t>(lo), static_cast<size_t>(hi - lo));
            selection_start_ = lo;
            selection_end_   = lo;
        }
        else if (selection_start_ < static_cast<int>(text_.size()))
        {
            text_.erase(static_cast<size_t>(selection_start_), 1);
        }
        else
        {
            return; // nothing to delete
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

    uint64_t TextEditingController::addListener(std::function<void()> fn)
    {
        uint64_t id = next_listener_id_++;
        listeners_.emplace_back(id, std::move(fn));
        return id;
    }

    void TextEditingController::removeListener(uint64_t id)
    {
        listeners_.erase(
            std::remove_if(listeners_.begin(), listeners_.end(),
                           [id](const auto& p) { return p.first == id; }),
            listeners_.end());
    }

    void TextEditingController::notifyListeners()
    {
        auto copy = listeners_; // snapshot in case a listener mutates the list
        for (auto& [lid, fn] : copy)
            fn();
    }

} // namespace systems::leal::campello_widgets
