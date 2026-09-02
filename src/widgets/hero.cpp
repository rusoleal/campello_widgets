#include <campello_widgets/widgets/hero.hpp>
#include <campello_widgets/widgets/positioned.hpp>
#include <campello_widgets/widgets/value_listenable_builder.hpp>
#include <campello_widgets/widgets/render_object_element.hpp>
#include <campello_widgets/ui/render_hero.hpp>
#include <campello_widgets/ui/post_frame_callbacks.hpp>
#include <campello_widgets/ui/tween.hpp>

namespace systems::leal::campello_widgets
{

    std::shared_ptr<RenderObject> Hero::createRenderObject() const
    {
        return std::make_shared<RenderHero>();
    }

    std::unordered_map<std::string, Element*> Hero::collectHeroesFor(Element* root)
    {
        std::unordered_map<std::string, Element*> result;
        Element::visitAllDescendants(root, [&](Element* e)
        {
            if (auto* hero = dynamic_cast<const Hero*>(&e->widget()))
                result[hero->tag] = e;
        });
        return result;
    }

    void HeroController::didChangeTop(
        Route* top_route, std::shared_ptr<AnimationController> animation, Route* previous_top_route)
    {
        if (!navigator() || !top_route || !previous_top_route || !animation) return;

        PostFrameCallbacks::schedule([this, top_route, previous_top_route, animation]() {
            buildManifestsAndFly(top_route, previous_top_route, animation);
        });
    }

    void HeroController::buildManifestsAndFly(
        Route* top_route, Route* previous_top_route, std::shared_ptr<AnimationController> animation)
    {
        manifests_.clear();
        if (!navigator()) return;

        Element* to_layer   = navigator()->elementForRoute(top_route);
        Element* from_layer = navigator()->elementForRoute(previous_top_route);
        if (!to_layer || !from_layer) return;

        auto from_heroes = Hero::collectHeroesFor(from_layer);
        auto to_heroes   = Hero::collectHeroesFor(to_layer);

        for (auto& [tag, from_element] : from_heroes)
        {
            auto it = to_heroes.find(tag);
            if (it == to_heroes.end()) continue;
            manifests_.push_back(FlightManifest{tag, from_element, it->second});
        }

        if (!manifests_.empty())
            runFlights(animation);
    }

    void HeroController::runFlights(std::shared_ptr<AnimationController> animation)
    {
        // Direction the shared route-transition controller is running:
        // push() calls forward(0.0) (value runs 0 -> 1), pop() calls
        // reverse() with no `from` (value runs 1 -> 0, from wherever it had
        // already reached). Tween<Rect>::evaluate(*animation) reads
        // normalizedValue() -- for a pop, that runs 1 -> 0 across the
        // transition, so the tween's begin/end must be swapped relative to
        // push, or the flight would visibly run backwards (starting at the
        // destination rect and ending at the source rect).
        const bool reversed = animation->status() == AnimationStatus::reverse;

        for (auto& m : manifests_)
        {
            auto* from_re = m.from_element->nearestRenderObjectElement();
            auto* to_re   = m.to_element->nearestRenderObjectElement();
            if (!from_re || !to_re) continue;

            auto* from_hero = dynamic_cast<RenderHero*>(from_re->renderObject());
            auto* to_hero   = dynamic_cast<RenderHero*>(to_re->renderObject());
            if (!from_hero || !to_hero) continue;

            from_hero->setHidden(true);
            to_hero->setHidden(true);

            // Re-read both endpoints' rects on every tick rather than
            // freezing them once at flight start. globalRect() is a plain
            // field read (RenderHero::performPaint() keeps it current every
            // frame regardless of hidden_, since only the child's own paint
            // is skipped) -- essentially free. This matters because the
            // pushed/popped route's own SlideTransition keeps moving its
            // content throughout the whole transition: capturing its Hero's
            // rect once, at the very start, would target wherever it
            // happened to be at that instant (e.g. still fully off-screen
            // for a push, t=0) instead of where it's actually heading.
            // Re-measuring lets the tween's moving endpoint track the
            // route's own transition and converge on the real resting rect.
            auto evaluateShuttleRect = [reversed](RenderHero& f, RenderHero& t, AnimationController& anim) {
                const Tween<Rect> tween = reversed ? Tween<Rect>{t.globalRect(), f.globalRect()}
                                                    : Tween<Rect>{f.globalRect(), t.globalRect()};
                return tween.evaluate(anim);
            };

            auto notifier = std::make_shared<ValueNotifier<Rect>>(
                evaluateShuttleRect(*from_hero, *to_hero, *animation));
            auto vlb = std::make_shared<ValueListenableBuilder<Rect>>();
            vlb->valueListenable = notifier;

            WidgetRef flying_child = static_cast<const Hero&>(m.to_element->widget()).child;
            vlb->builder = [flying_child](BuildContext&, const Rect& r, WidgetRef) -> WidgetRef {
                auto pos    = std::make_shared<Positioned>();
                pos->left   = r.x;
                pos->top    = r.y;
                pos->width  = r.width;
                pos->height = r.height;
                pos->child  = flying_child;
                return pos;
            };

            auto entry = OverlayEntry::create(vlb);
            Overlay::insert(entry);

            auto listener_id = std::make_shared<uint64_t>(0);
            *listener_id = animation->addListener(
                [animation, evaluateShuttleRect, notifier, entry, from_hero, to_hero, listener_id]() {
                    notifier->setValue(evaluateShuttleRect(*from_hero, *to_hero, *animation));
                    if (animation->status() == AnimationStatus::completed ||
                        animation->status() == AnimationStatus::dismissed)
                    {
                        Overlay::remove(entry);
                        to_hero->setHidden(false);
                        animation->removeListener(*listener_id);
                    }
                });

            active_flights_.push_back(ActiveFlight{m.tag, animation, entry, notifier});
        }
    }

} // namespace systems::leal::campello_widgets
