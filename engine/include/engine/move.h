#pragma once

#include <cstdint>
#include <string>

namespace skipbo {

enum class MoveSource : uint8_t {
    Hand0, Hand1, Hand2, Hand3, Hand4,
    StockPile,
    DiscardPile0, DiscardPile1, DiscardPile2, DiscardPile3
};

enum class MoveTarget : uint8_t {
    BuildingPile0, BuildingPile1, BuildingPile2, BuildingPile3,
    DiscardPile0, DiscardPile1, DiscardPile2, DiscardPile3
};

struct Move {
    MoveSource source;
    MoveTarget target;

    bool operator==(const Move& o) const {
        return source == o.source && target == o.target;
    }
    bool operator!=(const Move& o) const { return !(*this == o); }

    bool is_discard() const {
        return target >= MoveTarget::DiscardPile0;
    }

    bool is_from_hand() const {
        return source <= MoveSource::Hand4;
    }

    bool is_from_stock() const {
        return source == MoveSource::StockPile;
    }

    bool is_from_discard() const {
        return source >= MoveSource::DiscardPile0;
    }

    int hand_index() const {
        return static_cast<int>(source) - static_cast<int>(MoveSource::Hand0);
    }

    int source_discard_index() const {
        return static_cast<int>(source) - static_cast<int>(MoveSource::DiscardPile0);
    }

    int target_building_index() const {
        return static_cast<int>(target) - static_cast<int>(MoveTarget::BuildingPile0);
    }

    int target_discard_index() const {
        return static_cast<int>(target) - static_cast<int>(MoveTarget::DiscardPile0);
    }
};

std::string move_to_string(const Move& m);

// Fixed-capacity move list — avoids std::vector allocation in hot paths.
constexpr int MAX_LEGAL_MOVES = 64;

struct MoveList {
    Move moves[MAX_LEGAL_MOVES];
    uint8_t count = 0;

    MoveList() = default;

    void push_back(const Move& m) { moves[count++] = m; }
    bool empty() const { return count == 0; }
    int size() const { return count; }
    void clear() { count = 0; }

    Move& operator[](int i) { return moves[i]; }
    const Move& operator[](int i) const { return moves[i]; }

    Move* begin() { return moves; }
    Move* end() { return moves + count; }
    const Move* begin() const { return moves; }
    const Move* end() const { return moves + count; }

    // O(1) erase by swapping with last element (order not preserved)
    void swap_erase(int i) { moves[i] = moves[--count]; }
};

} // namespace skipbo
