add_executable(engine_tests
    test_card.cpp
    test_deck.cpp
    test_game_state.cpp
    test_rules.cpp
    test_game.cpp
)
target_link_libraries(engine_tests PRIVATE skipbo_engine skipbo_ai Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(engine_tests)
