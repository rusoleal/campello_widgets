#include <algorithm>
#include <limits>
#include <campello_widgets/ui/render_dropdown_menu_positioner.hpp>

namespace systems::leal::campello_widgets
{

    void RenderDropdownMenuPositioner::performLayout()
    {
        // Fill the incoming constraints — a Positioned::fill inside a
        // Stack::expand, so this equals the live overlay/viewport size.
        size_ = constraints_.constrain({constraints_.max_width, constraints_.max_height});

        if (!child_) return;

        constexpr float kGap = 4.0f;

        // Measure the menu's true natural size — loose width up to what's
        // available, unbounded height so it reports what it actually
        // needs rather than a guess.
        const BoxConstraints measure{
            0.0f, size_.width,
            0.0f, std::numeric_limits<float>::infinity(),
        };
        child_->layout(measure);
        const Size menu_size = child_->size();

        if (beside) {
            float bx = anchor_pos.x + anchor_size.width;
            if (bx + menu_size.width > size_.width) bx = anchor_pos.x - menu_size.width;
            bx = std::clamp(bx, 0.0f, std::max(0.0f, size_.width - menu_size.width));
            const float by = std::clamp(anchor_pos.y - beside_inset, 0.0f,
                                        std::max(0.0f, size_.height - menu_size.height));
            positionChild(*child_, {bx, by});
            return;
        }

        const float left = center_horizontally
            ? anchor_pos.x + (anchor_size.width - menu_size.width) / 2.0f
            : anchor_pos.x;
        const float x = std::clamp(left, 0.0f,
                                    std::max(0.0f, size_.width - menu_size.width));

        const float needed      = menu_size.height + kGap;
        const bool  fits_below  = anchor_pos.y + anchor_size.height + needed <= size_.height;
        const bool  fits_above  = anchor_pos.y - needed >= 0.0f;

        // Prefer below (the natural/expected direction) whenever it
        // actually fits, or when neither direction fully fits (clamped
        // onto screen below is more useful than clamped above). Only open
        // upward when below doesn't fit and above does.
        float y = (fits_below || !fits_above)
            ? anchor_pos.y + anchor_size.height + kGap
            : anchor_pos.y - menu_size.height - kGap;
        y = std::clamp(y, 0.0f, std::max(0.0f, size_.height - menu_size.height));

        positionChild(*child_, {x, y});
    }

} // namespace systems::leal::campello_widgets
