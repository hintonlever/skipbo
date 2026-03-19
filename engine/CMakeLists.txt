add_library(skipbo_engine STATIC
    src/card.cpp
    src/deck.cpp
    src/game_state.cpp
    src/rules.cpp
    src/game.cpp
)
target_include_directories(skipbo_engine PUBLIC include)
