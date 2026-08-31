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

if(APPLE)
    set_target_properties(
        pstk_packet_cli
        PROPERTIES
            INSTALL_RPATH "@loader_path" # 실행 파일이 있는 위치 기준 shared lib 탐색
    )
endif()

install(
    TARGETS pstk_packet_cli
    RUNTIME DESTINATION bin
)

install(
    FILES
        packet.ini
    DESTINATION bin
)

add_custom_target(
    pstk_packet_cli_config
    COMMAND
    "${CMAKE_COMMAND}"
    -E
    copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/packet.ini"
    "$<TARGET_FILE_DIR:pstk_packet_cli>/packet.ini"
    VERBATIM
)

add_dependencies(pstk_packet_cli pstk_packet_cli_config)

if(WIN32)
    add_custom_target(
        pstk_packet_cli_runtime
        COMMAND
        "${CMAKE_COMMAND}"
        -E
        make_directory
        "$<TARGET_FILE_DIR:pstk_packet_cli>"
        COMMAND
        "${CMAKE_COMMAND}"
        -E
        copy_if_different
        "$<TARGET_FILE:pstk_packet>"
        "$<TARGET_FILE_DIR:pstk_packet_cli>"
        VERBATIM
    )

    add_dependencies(pstk_packet_cli pstk_packet_cli_runtime)
endif()
