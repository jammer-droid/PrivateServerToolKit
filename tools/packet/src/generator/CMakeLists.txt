add_library(
    pstk_packet_generator_core
    STATIC
    TkPacketCompiler.cpp
    TkPacketGeneratedFileCommitter.cpp
)

target_compile_features(
    pstk_packet_generator_core
    PUBLIC
    cxx_std_17
)

target_link_libraries(
    pstk_packet_generator_core
    PUBLIC
    PSTK::Common
    pstk_packet_schema_compiler
)

target_include_directories(
    pstk_packet_generator_core
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

set_target_properties(
    pstk_packet_generator_core
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN TRUE
)

add_subdirectory(cpp)
add_subdirectory(csharp)
