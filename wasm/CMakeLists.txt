add_executable(skipbo_wasm bindings.cpp)
target_include_directories(skipbo_wasm PRIVATE ${CMAKE_SOURCE_DIR}/wasm)
target_link_libraries(skipbo_wasm PRIVATE skipbo_engine skipbo_ai)

set_target_properties(skipbo_wasm PROPERTIES
    OUTPUT_NAME "skipbo"
    SUFFIX ".js"
)

target_link_options(skipbo_wasm PRIVATE
    --bind
    "SHELL:-s MODULARIZE=1"
    "SHELL:-s EXPORT_NAME=createSkipBoModule"
    "SHELL:-s ENVIRONMENT=web,worker"
    "SHELL:-s ALLOW_MEMORY_GROWTH=1"
    "SHELL:-s WASM=1"
    "SHELL:-s NO_EXIT_RUNTIME=1"
    "SHELL:-s SINGLE_FILE=0"
)
