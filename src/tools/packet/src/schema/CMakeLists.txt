find_package(nlohmann_json CONFIG REQUIRED)

add_library(
    pstk_packet_schema_compiler
    STATIC
    TkPacketDiagnostic.cpp
    TkPacketJsonParser.cpp
    TkPacketDescriptorBuilder.cpp
    TkPacketSchemaCompiler.cpp
)

target_compile_features(
    pstk_packet_schema_compiler
    PUBLIC
    cxx_std_17
)

target_link_libraries(
    pstk_packet_schema_compiler

    PUBLIC
    PSTK::Common

    PRIVATE
    nlohmann_json::nlohmann_json
)

target_include_directories(
    pstk_packet_schema_compiler
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

set_target_properties(
    pstk_packet_schema_compiler
    PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN TRUE
)
