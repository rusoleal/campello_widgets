#pragma once

namespace systems::leal::campello_widgets
{

    /**
     * @brief Small shared mutable channel between a RenderSliverOverlapAbsorber
     * (outer viewport) and a RenderSliverOverlapInjector (inner viewport) --
     * Stage 2 of the NestedScrollView initiative, "the overlap channel" per
     * the NestedScrollView Scoping artifact.
     *
     * Constructed once by the caller (eventually the NestedScrollView
     * coordinator, a later stage) and passed by shared_ptr to both an
     * absorber and an injector, so the injector can reserve a gap matching
     * whatever the absorber's wrapped header content currently obstructs.
     */
    struct SliverOverlapAbsorberHandle
    {
        /** What the absorbed child(ren) permanently obstruct, once at rest. */
        float layout_extent = 0.0f;
    };

} // namespace systems::leal::campello_widgets
