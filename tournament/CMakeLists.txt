add_executable(skipbo_tournament
    main.cpp
    src/match.cpp
    src/elo.cpp
    src/tournament.cpp
)
target_include_directories(skipbo_tournament PRIVATE include)
target_link_libraries(skipbo_tournament PRIVATE skipbo_engine skipbo_ai)
