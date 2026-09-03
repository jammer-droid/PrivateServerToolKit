# 소스 파일로 실행 파일 target을 생성
add_executable(
    pstk_packet_smoke # 논리 target 이름
    TkPacketToolSmoke.cpp # 소스 파일 경로
)

# 링커가 사용할 라이브러리 지정
target_link_libraries(
    pstk_packet_smoke
    PRIVATE
    PSTK::Packet # Packet target alias 사용
)

# executable target을 CTest에 등록
add_test(
    NAME pstk.packet.api.version # CTest의 테스트 식별자

    # add_executable로 정의된 executable target을
    # CMake에서 실제 실행 파일 경로로 전달
    COMMAND pstk_packet_smoke
)

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

add_executable(
    pstk_packet_tests
    TkPacketCompilerTests.cpp
)

target_compile_features(
    pstk_packet_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_packet_tests
    PRIVATE
    PSTK::Packet
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_packet_tests
    TEST_PREFIX "pstk.packet.compiler."
)

add_executable(
    pstk_packet_csharp_generator_tests
    TkPacketCSharpGeneratorTests.cpp
)

target_compile_features(
    pstk_packet_csharp_generator_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_packet_csharp_generator_tests
    PRIVATE
    pstk_packet_csharp_generator
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_packet_csharp_generator_tests
    TEST_PREFIX "pstk.packet.csharp-generator."
)

add_executable(
    pstk_packet_schema_compiler_tests
    TkPacketSchemaCompilerTests.cpp
)

target_compile_features(
    pstk_packet_schema_compiler_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_packet_schema_compiler_tests
    PRIVATE
    pstk_packet_schema_compiler
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_packet_schema_compiler_tests
    TEST_PREFIX "pstk.packet.schema."
)

add_executable(
    pstk_packet_codec_support_tests
    TkPacketCodecSupportTests.cpp
)

target_compile_features(
    pstk_packet_codec_support_tests
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_packet_codec_support_tests
    PRIVATE
    PSTK::Packet
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_packet_codec_support_tests
    TEST_PREFIX "pstk.packet.codec-support."
)

set(PSTK_PACKET_GENERATED_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(PSTK_PACKET_GENERATED_TEST_HEADER "${PSTK_PACKET_GENERATED_TEST_DIR}/AllTypes.generated.h")
set(PSTK_PACKET_GENERATED_TEST_CSHARP_SOURCE "${PSTK_PACKET_GENERATED_TEST_DIR}/AllTypes.generated.cs")

# 패킷 생성 테스트
add_executable(
    pstk_packet_generate_test_sources
    TkPacketGenerateTestSources.cpp
)

target_compile_features(
    pstk_packet_generate_test_sources
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_packet_generate_test_sources
    PRIVATE
    pstk_packet_cpp_generator
    pstk_packet_csharp_generator
)

add_custom_command(
    OUTPUT
    "${PSTK_PACKET_GENERATED_TEST_HEADER}"
    "${PSTK_PACKET_GENERATED_TEST_CSHARP_SOURCE}"

    COMMAND
    ${CMAKE_COMMAND} -E make_directory "${PSTK_PACKET_GENERATED_TEST_DIR}"

    COMMAND
    pstk_packet_generate_test_sources               # generate packet test with
    "${PSTK_PACKET_GENERATED_TEST_HEADER}"          # cpp header
    "${PSTK_PACKET_GENERATED_TEST_CSHARP_SOURCE}"   # csharp source

    DEPENDS
    pstk_packet_generate_test_sources

    VERBATIM
)

add_executable(
    pstk_packet_generated_codec_tests
    TkPacketGeneratedCodecTests.cpp
    "${PSTK_PACKET_GENERATED_TEST_HEADER}"
)

target_compile_features(
    pstk_packet_generated_codec_tests
    PRIVATE
    cxx_std_17
)

target_include_directories(
    pstk_packet_generated_codec_tests
    PRIVATE
    "${PSTK_PACKET_GENERATED_TEST_DIR}"
)

target_link_libraries(
    pstk_packet_generated_codec_tests
    PRIVATE
    PSTK::Packet
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_packet_generated_codec_tests
    TEST_PREFIX "pstk.packet.generated-codec."
)

add_executable(
    pstk_packet_generate_time_sync_sources
    TkPacketGenerateTimeSyncSources.cpp
)

target_compile_features(
    pstk_packet_generate_time_sync_sources
    PRIVATE
    cxx_std_17
)

target_include_directories(
    pstk_packet_generate_time_sync_sources
    PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../include"
    "${CMAKE_CURRENT_BINARY_DIR}/.."
)

target_link_libraries(
    pstk_packet_generate_time_sync_sources
    PRIVATE
    pstk_packet_cpp_generator
)

set(PSTK_SERVICE_HOST_GENERATED_TEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/service-host-generated")
set(PSTK_SERVICE_HOST_GENERATED_REQUEST
    "${PSTK_SERVICE_HOST_GENERATED_TEST_DIR}/WorldTimeSyncRequest.generated.h")
set(PSTK_SERVICE_HOST_GENERATED_RESPONSE
    "${PSTK_SERVICE_HOST_GENERATED_TEST_DIR}/WorldTimeSyncResponse.generated.h")

add_custom_command(
    OUTPUT
    "${PSTK_SERVICE_HOST_GENERATED_REQUEST}"
    "${PSTK_SERVICE_HOST_GENERATED_RESPONSE}"

    COMMAND
    "${CMAKE_COMMAND}"
    -E
    make_directory
    "${PSTK_SERVICE_HOST_GENERATED_TEST_DIR}"

    COMMAND
    pstk_packet_generate_time_sync_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/../cli/tests/schemas/WorldTimeSyncRequest.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cli/tests/schemas/server/WorldTimeSyncResponse.json"
    "${PSTK_SERVICE_HOST_GENERATED_TEST_DIR}"

    DEPENDS
    pstk_packet_generate_time_sync_sources
    "${CMAKE_CURRENT_SOURCE_DIR}/../cli/tests/schemas/WorldTimeSyncRequest.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/../cli/tests/schemas/server/WorldTimeSyncResponse.json"

    VERBATIM
)

add_custom_target(
    pstk_service_host_time_sync_sources
    DEPENDS
    "${PSTK_SERVICE_HOST_GENERATED_REQUEST}"
    "${PSTK_SERVICE_HOST_GENERATED_RESPONSE}"
)

add_executable(
    pstk_service_host_time_sync_tests
    TkServiceHostTimeSyncTests.cpp
)

add_dependencies(
    pstk_service_host_time_sync_tests
    pstk_service_host_time_sync_sources
)

target_compile_features(
    pstk_service_host_time_sync_tests
    PRIVATE
    cxx_std_17
)

target_include_directories(
    pstk_service_host_time_sync_tests
    PRIVATE
    "${PSTK_SERVICE_HOST_GENERATED_TEST_DIR}"
)

target_link_libraries(
    pstk_service_host_time_sync_tests
    PRIVATE
    PSTK::ServiceHost
    PSTK::Packet
    GTest::gtest_main
)

gtest_discover_tests(
    pstk_service_host_time_sync_tests
    TEST_PREFIX "pstk.service-host.typed."
)
