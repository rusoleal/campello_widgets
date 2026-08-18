#pragma once

#include <memory>
#include <campello_widgets/widgets/stateless_widget.hpp>
#include <campello_widgets/ui/color.hpp>
#include <campello_widgets/diagnostics/debug_assert.hpp>

namespace systems::leal::campello_gpu { class Texture; }

namespace systems::leal::campello_widgets
{

    /**
     * @brief Displays a single-color glyph from a "template" texture.
     *
     * The source texture's own RGB is ignored entirely — only its alpha
     * channel is sampled and recolored with `color`, the same mechanism as
     * iOS's `UIImage.withRenderingMode(.alwaysTemplate)` and Android's
     * icon tinting. This lets one monochrome PNG asset per icon serve any
     * theme color instead of needing a separate pre-tinted asset per tint.
     *
     * `campello_widgets` core has no opinion on *which* icon set a glyph
     * comes from — `Icon` just paints an already-resolved texture. Each
     * `DesignSystem` implementation (`campello_cupertino`, `campello_material`)
     * owns its own semantic-name → texture registry (SF Symbols vs.
     * Material Symbols look and are licensed differently, so there's no
     * single cross-platform icon set to bake in here).
     *
     * @code
     * auto icon = Icon::create(icon_registry.resolve("house"), 24.0f, tokens.colors.primary);
     * @endcode
     */
    class Icon : public StatelessWidget
    {
    public:
        std::shared_ptr<campello_gpu::Texture> texture;

        /** @brief Side length in logical pixels of the (square) icon box. */
        float size = 24.0f;

        /** @brief Tint color — see the class doc comment. */
        Color color = Color::black();

        Icon() = default;

        static std::shared_ptr<Icon> create(
            std::shared_ptr<campello_gpu::Texture> texture,
            float                                  size  = 24.0f,
            Color                                  color = Color::black())
        {
            auto w      = std::make_shared<Icon>();
            w->texture  = std::move(texture);
            w->size     = size;
            w->color    = color;
            return w;
        }

        void debugValidate() const override
        {
            CW_ASSERT_MSG(size >= 0.0f, "Icon.size must be non-negative");
        }

        WidgetRef build(BuildContext& context) const override;
    };

} // namespace systems::leal::campello_widgets
