add_library(
    pstk_packet_cpp_generator
    STATIC
    TkPacketCppCompiler.cpp
    TkPacketCppGenerator.cpp
)

target_compile_features(
    pstk_packet_cpp_generator
    PUBLIC
    cxx_std_17
)

target_link_libraries(
    pstk_packet_cpp_generator
    PUBLIC
    PSTK::Common
    pstk_packet_schema_compiler
)

target_include_directories(
    pstk_packet_cpp_generator
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../..>

    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
    ${CMAKE_CURRENT_BINARY_DIR}/../../..
)

set_target_properties(
    pstk_packet_cpp_generator
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN TRUE
)
