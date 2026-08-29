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

add_executable(
    pstk_packet_generate_test_header
    TkPacketGenerateTestHeader.cpp
)

target_compile_features(
    pstk_packet_generate_test_header
    PRIVATE
    cxx_std_17
)

target_link_libraries(
    pstk_packet_generate_test_header
    PRIVATE
    pstk_packet_cpp_generator
)

add_custom_command(
    # custom command 생성 결과(생성된 헤더 파일)
    OUTPUT
    "${PSTK_PACKET_GENERATED_TEST_HEADER}"

    # 생성 헤더 넣을 build directory 생성
    COMMAND
    ${CMAKE_COMMAND} -E make_directory "${PSTK_PACKET_GENERATED_TEST_DIR}"

    # 테스트용 generator 실행 파일 호출 및 출력 경로를 인자로 전달
    COMMAND
    pstk_packet_generate_test_header "${PSTK_PACKET_GENERATED_TEST_HEADER}"

    # custom command 사용에 필요한 의존성 설정
    DEPENDS
    pstk_packet_generate_test_header

    # 경로 공백, 따옴표, platform 별 escape 등을 cmake에서 처리
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
