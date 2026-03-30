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
