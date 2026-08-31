find_package(SimpleIni CONFIG REQUIRED)

add_executable(
    pstk_packet_cli
    TkPacketCli.cpp
)

target_compile_features(
    pstk_packet_cli
    PRIVATE
    cxx_std_17
)

target_compile_definitions(
    pstk_packet_cli
    PRIVATE
    SI_CONVERT_GENERIC
)

target_link_libraries(
    pstk_packet_cli
    PRIVATE
    PSTK::Packet
    SimpleIni::SimpleIni
)

set_target_properties(
    pstk_packet_cli
    PROPERTIES
        OUTPUT_NAME pstk-packet
)

if(WIN32)
    add_custom_command(
        TARGET pstk_packet_cli
        POST_BUILD
        COMMAND
        "${CMAKE_COMMAND}"
        -E
        copy_if_different
        "$<TARGET_FILE:pstk_packet>"
        "$<TARGET_FILE_DIR:pstk_packet_cli>"
        VERBATIM
    )
endif()
