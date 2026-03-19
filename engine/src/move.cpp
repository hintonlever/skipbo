#include "engine/move.h"

namespace skipbo {

static std::string source_to_string(MoveSource s) {
    switch (s) {
        case MoveSource::Hand0: return "Hand[0]";
        case MoveSource::Hand1: return "Hand[1]";
        case MoveSource::Hand2: return "Hand[2]";
        case MoveSource::Hand3: return "Hand[3]";
        case MoveSource::Hand4: return "Hand[4]";
        case MoveSource::StockPile: return "Stock";
        case MoveSource::DiscardPile0: return "Discard[0]";
        case MoveSource::DiscardPile1: return "Discard[1]";
        case MoveSource::DiscardPile2: return "Discard[2]";
        case MoveSource::DiscardPile3: return "Discard[3]";
    }
    return "?";
}

static std::string target_to_string(MoveTarget t) {
    switch (t) {
        case MoveTarget::BuildingPile0: return "Build[0]";
        case MoveTarget::BuildingPile1: return "Build[1]";
        case MoveTarget::BuildingPile2: return "Build[2]";
        case MoveTarget::BuildingPile3: return "Build[3]";
        case MoveTarget::DiscardPile0: return "Discard[0]";
        case MoveTarget::DiscardPile1: return "Discard[1]";
        case MoveTarget::DiscardPile2: return "Discard[2]";
        case MoveTarget::DiscardPile3: return "Discard[3]";
    }
    return "?";
}

std::string move_to_string(const Move& m) {
    return source_to_string(m.source) + " -> " + target_to_string(m.target);
}

} // namespace skipbo
