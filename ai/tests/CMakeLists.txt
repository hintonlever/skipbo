add_executable(ai_tests
    test_random_player.cpp
    test_determinizer.cpp
)
target_link_libraries(ai_tests PRIVATE skipbo_ai skipbo_engine Catch2::Catch2WithMain)

include(Catch)
catch_discover_tests(ai_tests)
