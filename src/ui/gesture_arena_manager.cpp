#include <campello_widgets/ui/gesture_arena_manager.hpp>

#include <algorithm>

namespace systems::leal::campello_widgets
{

    void GestureArenaEntry::resolve(GestureDisposition disposition)
    {
        manager_.resolve(pointer_id_, member_, disposition);
    }

    // -------------------------------------------------------------------------

    GestureArenaEntry GestureArenaManager::add(int32_t pointer_id, GestureArenaMember* member)
    {
        arenas_[pointer_id].members.push_back(member);
        return GestureArenaEntry(*this, pointer_id, member);
    }

    void GestureArenaManager::close(int32_t pointer_id)
    {
        auto it = arenas_.find(pointer_id);
        if (it == arenas_.end())
            return; // already resolved

        ArenaState& state = it->second;
        state.is_open     = false;
        tryToResolveArena(pointer_id, state);
    }

    void GestureArenaManager::sweep(int32_t pointer_id)
    {
        auto it = arenas_.find(pointer_id);
        if (it == arenas_.end())
            return; // already resolved

        ArenaState state = std::move(it->second);
        arenas_.erase(it);

        if (state.members.empty())
            return;

        state.members.front()->acceptGesture(pointer_id);
        for (size_t i = 1; i < state.members.size(); ++i)
            state.members[i]->rejectGesture(pointer_id);
    }

    void GestureArenaManager::resolve(int32_t pointer_id, GestureArenaMember* member, GestureDisposition disposition)
    {
        auto it = arenas_.find(pointer_id);
        if (it == arenas_.end())
            return; // already resolved

        ArenaState& state = it->second;
        state.members.erase(std::remove(state.members.begin(), state.members.end(), member), state.members.end());

        if (disposition == GestureDisposition::rejected)
        {
            member->rejectGesture(pointer_id);
            if (!state.is_open)
                tryToResolveArena(pointer_id, state);
            return;
        }

        // accepted
        if (state.eager_winner)
        {
            // Someone already claimed an eager win for this pointer; a later
            // accept from another member loses immediately.
            member->rejectGesture(pointer_id);
            return;
        }

        if (state.is_open)
        {
            state.eager_winner = member;
        }
        else
        {
            resolveInFavorOf(pointer_id, state, member);
        }
    }

    void GestureArenaManager::tryToResolveArena(int32_t pointer_id, ArenaState& state)
    {
        // eager_winner takes priority even if it has already been removed
        // from `members` (resolve() removes accepting members immediately).
        if (state.eager_winner)
        {
            resolveInFavorOf(pointer_id, state, state.eager_winner);
        }
        else if (state.members.size() == 1)
        {
            GestureArenaMember* winner = state.members.front();
            arenas_.erase(pointer_id);
            winner->acceptGesture(pointer_id);
        }
        else if (state.members.empty())
        {
            arenas_.erase(pointer_id);
        }
    }

    void GestureArenaManager::removeMember(GestureArenaMember* member) noexcept
    {
        for (auto& [id, state] : arenas_)
        {
            auto& members = state.members;
            members.erase(
                std::remove(members.begin(), members.end(), member),
                members.end());
            if (state.eager_winner == member)
                state.eager_winner = nullptr;
        }
    }

    void GestureArenaManager::resolveInFavorOf(int32_t pointer_id, ArenaState& state, GestureArenaMember* member)
    {
        std::vector<GestureArenaMember*> losers;
        losers.reserve(state.members.size());
        for (auto* m : state.members)
            if (m != member)
                losers.push_back(m);

        arenas_.erase(pointer_id);

        for (auto* loser : losers)
            loser->rejectGesture(pointer_id);
        member->acceptGesture(pointer_id);
    }

} // namespace systems::leal::campello_widgets
