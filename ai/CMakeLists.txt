add_library(skipbo_ai STATIC
    src/random_player.cpp
    src/heuristic_player.cpp
    src/heuristic_random_discard_player.cpp
    src/mcts_node.cpp
    src/mcts_player.cpp
    src/determinizer.cpp
    src/turn_action.cpp
)
target_include_directories(skipbo_ai PUBLIC include)
target_link_libraries(skipbo_ai PUBLIC skipbo_engine)
