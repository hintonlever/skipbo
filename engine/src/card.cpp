#include "engine/card.h"

namespace skipbo {

std::string card_to_string(Card c) {
    if (c == CARD_NONE) return "[]";
    if (c == CARD_SKIPBO) return "SB";
    return std::to_string(static_cast<int>(c));
}

} // namespace skipbo
